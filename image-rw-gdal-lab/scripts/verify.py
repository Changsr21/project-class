"""独立校验：用 GDAL Python 读取输入/输出，逐像素检查 out == max(in - SUB, 0)，并生成统计与对比图。"""
import sys, json, hashlib
import numpy as np
from osgeo import gdal
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

gdal.UseExceptions()
inp = sys.argv[1] if len(sys.argv) > 1 else "data/test.tif"
out = sys.argv[2] if len(sys.argv) > 2 else "output/test1.tif"
sub = int(sys.argv[3]) if len(sys.argv) > 3 else 10
resdir = sys.argv[4] if len(sys.argv) > 4 else "results"

def load(p):
    ds = gdal.Open(p)
    a = ds.ReadAsArray()
    meta = dict(width=ds.RasterXSize, height=ds.RasterYSize, bands=ds.RasterCount,
                dtype=gdal.GetDataTypeName(ds.GetRasterBand(1).DataType),
                compression=ds.GetMetadata("IMAGE_STRUCTURE").get("COMPRESSION"))
    if a.ndim == 2: a = a[None]
    return a, meta

a, ma = load(inp)
b, mb = load(out)
expected = np.clip(a.astype(np.int32) - sub, 0, None).astype(a.dtype)
mism = int((expected != b).sum())
names = ["R", "G", "B"] if a.shape[0] == 3 else [f"band{i+1}" for i in range(a.shape[0])]
report = {
    "input": inp, "output": out, "subtract": sub,
    "input_meta": ma, "output_meta": mb,
    "shape_equal": a.shape == b.shape,
    "total_values": int(a.size),
    "mismatched_values": mism,
    "input_values_below_sub (clamped to 0)": int((a < sub).sum()),
    "per_band": {n: {"in_min": int(a[i].min()), "in_max": int(a[i].max()), "in_mean": round(float(a[i].mean()), 4),
                     "out_min": int(b[i].min()), "out_max": int(b[i].max()), "out_mean": round(float(b[i].mean()), 4),
                     "mean_diff": round(float(a[i].mean() - b[i].mean()), 4)} for i, n in enumerate(names)},
    "md5_output_pixels": hashlib.md5(b.tobytes()).hexdigest(),
    "PASS": bool(a.shape == b.shape and mism == 0),
}
print(json.dumps(report, indent=2, ensure_ascii=False))
with open(f"{resdir}/verify_report.json", "w") as f:
    json.dump(report, f, indent=2, ensure_ascii=False)

# 对比图（降采样 1/8 显示）
s = 8  # 4096x3072 -> 512x384
def rgb(x): return np.transpose(x[:3, ::s, ::s], (1, 2, 0)) if x.shape[0] >= 3 else x[0, ::s, ::s]
fig, ax = plt.subplots(1, 2, figsize=(14, 5.6))
ax[0].imshow(rgb(a), cmap="gray"); ax[0].set_title("test.tif (input)"); ax[0].axis("off")
ax[1].imshow(rgb(b), cmap="gray"); ax[1].set_title(f"test1.tif (input - {sub}, clamp at 0)"); ax[1].axis("off")
plt.tight_layout(); plt.savefig(f"{resdir}/compare.png", dpi=110); plt.close()

# 纯文本 SVG 版对比图（内嵌 JPEG 缩略图），方便以文本形式提交到 GitHub
import base64, io
from PIL import Image
def thumb_b64(x):
    im = Image.fromarray(np.ascontiguousarray(rgb(x)).astype(np.uint8)).resize((512, 384))
    buf = io.BytesIO(); im.save(buf, "JPEG", quality=80); return base64.b64encode(buf.getvalue()).decode()
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="1044" height="420">'
       f'<rect width="100%" height="100%" fill="white"/>'
       f'<text x="256" y="22" text-anchor="middle" font-family="sans-serif" font-size="16">test.tif (input)</text>'
       f'<text x="788" y="22" text-anchor="middle" font-family="sans-serif" font-size="16">test1.tif (input - {sub}, clamp at 0)</text>'
       f'<image x="0" y="32" width="512" height="384" xlink:href="data:image/jpeg;base64,{thumb_b64(a)}"/>'
       f'<image x="532" y="32" width="512" height="384" xlink:href="data:image/jpeg;base64,{thumb_b64(b)}"/></svg>')
open(f"{resdir}/compare.svg", "w").write(svg)

# 直方图
fig, axs = plt.subplots(1, a.shape[0], figsize=(5 * a.shape[0], 3.6), squeeze=False)
bins = np.arange(0, 257)
for i, n in enumerate(names):
    axs[0, i].hist(a[i].ravel(), bins=bins, histtype="step", label="test.tif")
    axs[0, i].hist(b[i].ravel(), bins=bins, histtype="step", label="test1.tif")
    axs[0, i].set_title(f"band {n}"); axs[0, i].set_xlabel("value"); axs[0, i].legend()
plt.tight_layout(); plt.savefig(f"{resdir}/histogram.png", dpi=110); plt.savefig(f"{resdir}/histogram.svg"); plt.close()
sys.exit(0 if report["PASS"] else 1)
