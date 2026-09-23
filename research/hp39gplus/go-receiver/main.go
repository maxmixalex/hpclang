package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"time"
)

const (
	protocolVersion = 1
	flashBytes      = 0x100000
	blockBytes      = 0x4000
	dataBytes       = 56
	fnvOffset       = 2166136261
	fnvPrime        = 16777619
	passWord        = 0x50415353
	readTimeout     = 5 * time.Second
)

type reportDevice interface {
	WriteReport([]byte) error
	ReadReport(time.Duration) ([]byte, error)
	Close() error
	Product() string
}

func put32(b []byte, off int, v uint32) {
	binary.LittleEndian.PutUint32(b[off:off+4], v)
}

func get32(b []byte, off int) uint32 {
	return binary.LittleEndian.Uint32(b[off : off+4])
}

func command(cmd byte, block byte, offset uint32) []byte {
	b := make([]byte, 64)
	b[0] = cmd
	b[1] = protocolVersion
	copy(b[2:5], []byte("H39"))
	b[5] = block
	put32(b, 8, offset)
	return b
}

func fnv1a(data []byte) uint32 {
	h := uint32(fnvOffset)
	for _, x := range data {
		h = (h ^ uint32(x)) * fnvPrime
	}
	return h
}

func blockSum(data []byte) uint32 {
	var sum uint32
	for _, x := range data {
		sum += uint32(x)
	}
	return sum
}

func receiveImage(dev reportDevice, progress io.Writer) ([]byte, uint32, error) {
	if err := dev.WriteReport(command('?', 0, 0)); err != nil {
		return nil, 0, fmt.Errorf("send INFO request: %w", err)
	}
	info, err := dev.ReadReport(readTimeout)
	if err != nil {
		return nil, 0, fmt.Errorf("read INFO: %w", err)
	}
	if len(info) != 64 || !bytes.Equal(info[:8], []byte{'I', 1, 'H', '3', '9', 'R', 'O', 'M'}) {
		return nil, 0, fmt.Errorf("unexpected INFO report")
	}
	if get32(info, 8) != flashBytes || get32(info, 12) != blockBytes || get32(info, 16) != dataBytes {
		return nil, 0, fmt.Errorf(
			"protocol geometry mismatch: %#x/%#x/%d",
			get32(info, 8), get32(info, 12), get32(info, 16),
		)
	}

	image := make([]byte, flashBytes)
	var totalSum uint32
	if err := dev.WriteReport(command('S', 0, 0)); err != nil {
		return nil, 0, fmt.Errorf("start stream: %w", err)
	}

	blocks := flashBytes / blockBytes
	for block := 0; block < blocks; block++ {
		base := block * blockBytes
		received := make([]byte, 0, blockBytes)
		expectedSeq := uint16(0)
		bad := false

		for {
			p, err := dev.ReadReport(readTimeout)
			if err != nil {
				return nil, 0, fmt.Errorf("block %02X: read report: %w", block, err)
			}
			if len(p) != 64 {
				return nil, 0, fmt.Errorf("block %02X: short report %d", block, len(p))
			}

			switch p[0] {
			case 'D':
				n := int(p[1])
				seq := binary.LittleEndian.Uint16(p[2:4])
				off := int(get32(p, 4))
				if n < 1 || n > dataBytes {
					bad = true
					continue
				}
				if bad {
					continue
				}
				if seq != expectedSeq || off != base+len(received) {
					bad = true
					fmt.Fprintf(progress, "Block %02X: sequence gap; will retry at checkpoint\n", block)
					continue
				}
				received = append(received, p[8:8+n]...)
				expectedSeq++
				if len(received) > blockBytes {
					bad = true
				}

			case 'C':
				cblock := int(p[2])
				coff := int(get32(p, 4))
				clen := int(get32(p, 8))
				csum := get32(p, 12)
				cfnv := get32(p, 16)
				ok := !bad &&
					cblock == block &&
					coff == base &&
					clen == blockBytes &&
					len(received) == blockBytes &&
					blockSum(received) == csum &&
					fnv1a(received) == cfnv
				if !ok {
					fmt.Fprintf(progress, "Block %02X: validation failed; requesting automatic retry\n", block)
					if err := dev.WriteReport(command('R', byte(block), uint32(base))); err != nil {
						return nil, 0, fmt.Errorf("block %02X: request retry: %w", block, err)
					}
					received = received[:0]
					expectedSeq = 0
					bad = false
					continue
				}

				copy(image[base:base+blockBytes], received)
				totalSum += csum
				if err := dev.WriteReport(command('A', byte(block), uint32(base))); err != nil {
					return nil, 0, fmt.Errorf("block %02X: acknowledge: %w", block, err)
				}
				fmt.Fprintf(progress, "Block %02d/%d: OK\n", block+1, blocks)
				goto nextBlock

			default:
				return nil, 0, fmt.Errorf("block %02X: unexpected report type 0x%02x", block, p[0])
			}
		}
	nextBlock:
	}

	end, err := dev.ReadReport(readTimeout)
	if err != nil {
		return nil, 0, fmt.Errorf("read END: %w", err)
	}
	if len(end) != 64 || !bytes.Equal(end[:8], []byte{'E', 1, 'H', '3', '9', 'R', 'O', 'M'}) {
		return nil, 0, fmt.Errorf("unexpected END report")
	}
	if get32(end, 8) != flashBytes || get32(end, 12) != totalSum || get32(end, 16) != passWord {
		return nil, 0, errors.New("END summary mismatch")
	}
	return image, totalSum, nil
}

func run() (retErr error) {
	output := "hp39gplus-flash.bin"
	wait := 120 * time.Second
	flag.StringVar(&output, "o", output, "output ROM image")
	flag.DurationVar(&wait, "wait", wait, "time to wait for HP39g+ ROM Stream to enumerate")
	flag.Parse()
	if flag.NArg() > 1 {
		return errors.New("usage: hp39gplus-rom-receiver.exe [-wait 120s] [-o image.bin] [image.bin]")
	}
	if flag.NArg() == 1 {
		output = flag.Arg(0)
	}

	fmt.Println("Waiting for HP39g+ ROM Stream...")
	dev, err := openStreamer(wait)
	if err != nil {
		return err
	}
	finished := false
	defer func() {
		if !finished {
			_ = dev.WriteReport(command('X', 0, 0))
			fmt.Fprintln(os.Stderr, "Receiver aborted. Unplug the calculator USB cable now so the aplet can restore hardware state and return to the stock OS.")
		}
		if err := dev.Close(); retErr == nil && err != nil {
			retErr = err
		}
	}()

	product := dev.Product()
	if product == "" {
		product = "HP39g+ ROM Stream"
	}
	fmt.Printf("Connected: %s\n", product)

	image, totalSum, err := receiveImage(dev, os.Stdout)
	if err != nil {
		return err
	}

	sum := sha256.Sum256(image)
	digest := hex.EncodeToString(sum[:])
	if err := os.WriteFile(output, image, 0o644); err != nil {
		return fmt.Errorf("write ROM image: %w", err)
	}
	manifest := map[string]any{
		"protocol":   "H39ROM-HID/1",
		"bytes":      len(image),
		"sha256":     digest,
		"byte_sum32": fmt.Sprintf("0x%08x", totalSum),
		"vid":        "03f0",
		"pid":        "0121",
	}
	data, err := json.MarshalIndent(manifest, "", "  ")
	if err != nil {
		return fmt.Errorf("encode manifest: %w", err)
	}
	data = append(data, '\n')
	if err := os.WriteFile(output+".json", data, 0o644); err != nil {
		return fmt.Errorf("write manifest: %w", err)
	}

	fmt.Printf("ROM saved: %s (%d bytes)\n", output, len(image))
	fmt.Printf("SHA-256: %s\n", digest)
	if err := dev.WriteReport(command('F', 0, 0)); err != nil {
		return fmt.Errorf("send FINISH: %w", err)
	}
	finished = true
	fmt.Println("Dump complete. Unplug the calculator USB cable now; the aplet will then restore hardware state and return to the stock OS.")
	return nil
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
