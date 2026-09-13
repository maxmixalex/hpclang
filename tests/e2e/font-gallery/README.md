# Font Gallery E2E

Font Gallery is the calculator-side visual regression gate for hpclang fonts.

- The **full** profile links every generated face in `runtime/fonts/generated` plus the two calculator-native faces and is exercised by Direct E2E on the hp39, hp48, and hp4950 families.
- The **stock-smoke** profile links one generated face plus the two native faces and validates the real calculator UI → ARMToolbox → HPC1 launch path on HP48gII and HP50g.
- HP48/49/50 native-font correctness is intentionally a Stock concern because the Direct synthetic launcher does not fabricate a live Saturn `Read[]` mapping. HP39/40 native fonts use the independently verified physical-ROM path and can therefore be validated in Direct.
- Catalog size is derived from the generated font index at configure time. Do not hard-code historical face or capture counts in the test implementation; source-package curation is expected to change them.

The human asset-review/audit path is separate: `tools/audit-font-assets.py` checks canonical-source ↔ generated semantic parity, while `tools/render-font-audit-cn.py` produces the wide visual review report used for font-family curation.
