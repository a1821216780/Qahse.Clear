"""Convert WindL .qwd template to properly nested YAML format"""
import re
from pathlib import Path

SRC = Path(r"E:\Qahse.Clear\demo\WindL\Qahse_WindL_Batch_IEC61400_3_1_NREL5MW.qwd")
DST = Path(r"D:/WindL_YamlTest\IEC3_1_template.yml")

lines = SRC.read_text(encoding="utf-8").splitlines()

def yaml_value(val):
    """Convert QWD value token to proper YAML value"""
    if val.lower() in ("true", "false"):
        return val.lower()
    if re.match(r"^-?\d+\.?\d*$", val):
        return val
    if val.lower() == "default":
        return "default"
    if val.startswith('"') and val.endswith('"'):
        inner = val[1:-1].replace("\\", "/")
        specials = set(' #{}[],&*?|<=>!%@`')
        if any(c in inner for c in specials):
            return f'"{inner}"'
        return inner
    return val

children = []
for line in lines:
    stripped = line.strip()
    if not stripped or stripped.startswith("--") or stripped.startswith("#"):
        continue
    parts = stripped.split(None, 2)
    if len(parts) < 2:
        continue
    val, key = parts[0], parts[1]
    if key.startswith("-") and len(key) > 5:
        continue
    children.append(f"    {key}: {yaml_value(val)}")

yml_out = ["Qahse:", "  WindL:"] + children
DST.write_text("\n".join(yml_out), encoding="utf-8")
print(f"Generated {DST}  ({len(yml_out)} lines)")
for l in yml_out[:18]:
    print(f"  {l}")
print("  ...")
