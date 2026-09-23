package main

import (
	"bytes"
	"encoding/binary"
	"strings"
	"testing"
	"time"
)

type fakeDevice struct {
	block     int
	pos       int
	mode      string
	retryDone bool
}

func (d *fakeDevice) Product() string { return "HP39g+ ROM Stream" }
func (d *fakeDevice) Close() error     { return nil }

func fakeByte(off int) byte {
	return byte((off*17 + 3) & 0xff)
}

func fakeBlock(block int) []byte {
	base := block * blockBytes
	out := make([]byte, blockBytes)
	for i := range out {
		out[i] = fakeByte(base + i)
	}
	return out
}

func (d *fakeDevice) WriteReport(p []byte) error {
	switch p[0] {
	case '?':
		d.mode = "info"
	case 'S':
		d.block = 0
		d.pos = 0
		d.mode = "data"
	case 'A':
		if int(p[5]) != d.block || int(get32(p, 8)) != d.block*blockBytes {
			panic("bad ACK")
		}
		d.block++
		d.pos = 0
		if d.block == flashBytes/blockBytes {
			d.mode = "end"
		} else {
			d.mode = "data"
		}
	case 'R':
		if int(p[5]) != d.block {
			panic("bad RETRY")
		}
		d.pos = 0
		d.mode = "data"
		d.retryDone = true
	case 'F':
		d.mode = "finished"
	case 'X':
		d.mode = "aborted"
	}
	return nil
}

func (d *fakeDevice) ReadReport(time.Duration) ([]byte, error) {
	b := make([]byte, 64)
	switch d.mode {
	case "info":
		copy(b[:8], []byte{'I', 1, 'H', '3', '9', 'R', 'O', 'M'})
		put32(b, 8, flashBytes)
		put32(b, 12, blockBytes)
		put32(b, 16, dataBytes)
		d.mode = "idle"
		return b, nil
	case "data":
		if d.pos >= blockBytes {
			d.mode = "checkpoint"
			return d.ReadReport(0)
		}
		n := dataBytes
		if left := blockBytes - d.pos; left < n {
			n = left
		}
		off := d.block*blockBytes + d.pos
		b[0] = 'D'
		b[1] = byte(n)
		binary.LittleEndian.PutUint16(b[2:4], uint16(d.pos/dataBytes))
		put32(b, 4, uint32(off))
		for i := 0; i < n; i++ {
			b[8+i] = fakeByte(off + i)
		}
		if d.block == 3 && !d.retryDone && d.pos == dataBytes*10 {
			b[8] ^= 0x55
		}
		d.pos += n
		if d.pos >= blockBytes {
			d.mode = "checkpoint"
		}
		return b, nil
	case "checkpoint":
		block := fakeBlock(d.block)
		b[0] = 'C'
		b[1] = 1
		b[2] = byte(d.block)
		put32(b, 4, uint32(d.block*blockBytes))
		put32(b, 8, blockBytes)
		put32(b, 12, blockSum(block))
		put32(b, 16, fnv1a(block))
		d.mode = "await-ack"
		return b, nil
	case "end":
		copy(b[:8], []byte{'E', 1, 'H', '3', '9', 'R', 'O', 'M'})
		put32(b, 8, flashBytes)
		var total uint32
		for i := 0; i < flashBytes/blockBytes; i++ {
			total += blockSum(fakeBlock(i))
		}
		put32(b, 12, total)
		put32(b, 16, passWord)
		d.mode = "await-finish"
		return b, nil
	default:
		panic("read in fake state " + d.mode)
	}
}

func TestReceiveImageRetryAndExactBytes(t *testing.T) {
	dev := &fakeDevice{}
	var progress bytes.Buffer
	image, _, err := receiveImage(dev, &progress)
	if err != nil {
		t.Fatal(err)
	}
	expected := make([]byte, flashBytes)
	for i := range expected {
		expected[i] = fakeByte(i)
	}
	if !bytes.Equal(image, expected) {
		t.Fatal("receiver produced the wrong 1 MiB image")
	}
	if !strings.Contains(progress.String(), "validation failed; requesting automatic retry") {
		t.Fatal("receiver did not exercise automatic retry")
	}
}
