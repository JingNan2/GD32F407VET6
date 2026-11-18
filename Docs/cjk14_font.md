# 外置 Flash 中文字库 `cjk14.bin`

文件浏览器用 LVGL **bin 字库**，存在 EFS（外置 SPI Flash），路径：`E:cjk14.bin`。  
按需读字形（`dynamic_glyph_load`），不占片内 Flash / 大块 RAM。

FatFs 已配置 `FF_LFN_UNICODE=2`（UTF-8 文件名），与 LVGL 一致。

## 1. 生成 `cjk14.bin`（PC）

依赖：

- Node.js + `lv_font_conv`
- 字体：思源黑体（仓库已备 `Assets/fonts/SourceHanSansCN-Normal.otf`；国内可用 `fastly.jsdelivr.net` 镜像）

```bash
npm i -g lv_font_conv
```

字集文件：[Assets/cjk14_symbols.txt](../Assets/cjk14_symbols.txt)（与 LVGL 内置 SourceHan SC 14 CJK 子集一致）。

推荐用仓库脚本（内部读字集后传给 `--symbols`）：

```powershell
.\Tools\gen_cjk14_bin.ps1 -FontPath "D:\fonts\SourceHanSansSC-Normal.otf"
```

或手动（PowerShell，注意 `--symbols` 要整段 UTF-8 字符串）：

```powershell
$sym = [IO.File]::ReadAllText("Assets\cjk14_symbols.txt", [Text.UTF8Encoding]::new($false))
lv_font_conv --font Assets\fonts\SourceHanSansCN-Normal.otf --size 14 --bpp 1 `
  --no-compress --no-prefilter -r 0x20-0x7F --symbols $sym `
  --format bin -o Assets\cjk14.bin
```

`--bpp 1` 约 45KB，整库进 LVGL RAM（`LV_MEM_SIZE=72KB`）。字库在 LVGL 任务里加载，不要在 `app_init` 主栈加载（会 HardFault）。

## 2. 烧到板子（串口 WriteFile）

1. 串口 460800，打开终端。
2. 发送：`/WriteFile cjk14.bin`
3. 再以 **二进制** 发送 `Assets/cjk14.bin` 全文（不要文本模式）。
4. 等到 `WriteFile OK ...`。
5. `/ListFiles` 应能看到 `cjk14.bin`。
6. 复位或点 UI **Refresh**；串口应有：`[UI] CJK font loaded E:cjk14.bin (dynamic)`。

未烧字库时 UI 仍可用，仅中文为缺字占位；串口会提示：

`[UI] CJK font missing: /WriteFile cjk14.bin then reboot`

## 3. 验证中文文件名

1. PC 往 TF（exFAT）拷贝带中文名的文件。
2. 插卡 → 点 **TF** / **Refresh**。
3. 列表应显示正确中文（字库含该字时）；串口 `[UI] TF #n ...` 在 UTF-8 终端下也应可读。

## 说明

- EFS 文件名最长 28 字节；固定用 `cjk14.bin`。
- Flash 与 LCD 共用 SPI0，画中文时会穿插读外置 Flash，列表刷新可能略慢，属预期。
- 若需更多汉字：扩大 `cjk14_symbols.txt` 后重新 `lv_font_conv`，再 `/WriteFile cjk14.bin` 覆盖。
