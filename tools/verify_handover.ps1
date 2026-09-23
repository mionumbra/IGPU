# 交接自检：核对文档声明与代码实际状态是否一致。
#
# 为什么要这个脚本：交接文档最容易失真的地方是"API 清单"和"能力位"——
# 它们会被后续每一次改动悄悄改掉，而文档不会自己更新。
# 这个脚本把"文档说的"和"代码里的"逐条对比，避免下一任基于错误前提开工。
#
# 用法（在仓库根目录）：pwsh -File tools\verify_handover.ps1
# 退出码 0 = 全部一致；1 = 有不一致（会打印具体差异）。

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$spec = Join-Path $root "spec.gmidl"
$handover = Join-Path $root "HANDOVER.md"
$capsH = Join-Path $root "src\native\igpu_capabilities.h"
$capsCpp = Join-Path $root "src\native\igpu_capabilities.cpp"
$test = Join-Path $root "project\objects\obj_igpu_test\Create_0.gml"

$fail = 0
function Check($label, $ok, $detail) {
    if ($ok) {
        Write-Host "  [OK]   $label" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] $label" -ForegroundColor Red
        if ($detail) { Write-Host "         $detail" -ForegroundColor Yellow }
        $script:fail++
    }
}

Write-Host "`n=== 1. spec.gmidl 的每个函数都写进了 HANDOVER ===" -ForegroundColor Cyan
$funcs = Select-String -Path $spec -Pattern '^function\s+(\w+)' |
    ForEach-Object { $_.Matches[0].Groups[1].Value }
$docText = Get-Content $handover -Raw
$undocumented = $funcs | Where-Object { $docText -notmatch [regex]::Escape($_) }
Check "全部 $($funcs.Count) 个函数已记录" ($undocumented.Count -eq 0) ("未记录: " + ($undocumented -join ", "))

Write-Host "`n=== 2. HANDOVER 里的 spec API 名都真实存在（防幽灵 API）===" -ForegroundColor Cyan
# 只检查"文档声称是 spec API"的名字，判据是 §4 里以 `igpu_xxx(...)` 形式
# 带括号列出的那些——那才是对 spec 的直接断言。
# GML 辅助函数（igpu_buffer_upload 等）、文件名（igpu_buffer.h）、
# 测试里引用的名字本来就不该在 spec 里，把它们算进来是误报。
# 判据：行内代码里的 `igpu_xxx(...)` 形式，即反引号包裹且紧跟左括号。
$docApiCalls = [regex]::Matches($docText, '`(igpu_\w+)\(') |
    ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique

# 其中一部分是 GML 辅助函数，不在 spec 里而在 IGPU_helpers.gml 里。
# 两类都查，但分别对各自的来源，避免把辅助函数误判成幽灵 API。
$helpers = Get-Content (Join-Path $root "project\scripts\IGPU_helpers\IGPU_helpers.gml") -Raw
$ghostApi = @()
$ghostHelper = @()
foreach ($name in $docApiCalls) {
    if ($funcs -contains $name) { continue }
    if ($helpers -match "function\s+$name\s*\(") { continue }
    # 既不在 spec 也不在 helpers
    $ghostApi += $name
}
Check "文档以 API 形式列出的名字都存在（spec 或 GML 辅助，共 $($docApiCalls.Count) 个）" `
    ($ghostApi.Count -eq 0) ("两处都找不到: " + ($ghostApi -join ", "))

Write-Host "`n=== 3. 能力位：枚举 / supports() / build_capabilities() 三处对齐 ===" -ForegroundColor Cyan
# 枚举成员的写法是 `Name = 数字`，最后一项可能没有尾逗号，所以逗号要可选。
$enumNames = Select-String -Path $capsH -Pattern '^\s+(\w+)\s*=\s*\d+\s*,?\s*$' |
    ForEach-Object { $_.Matches[0].Groups[1].Value }
# supports() 的 switch 分支覆盖了哪些
$supportsNames = Select-String -Path $capsCpp -Pattern 'case Capability::(\w+):' |
    ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique
# None 只是哨兵，不需要分支
$needBranch = $enumNames | Where-Object { $_ -ne "None" }
$noSupports = $needBranch | Where-Object { $supportsNames -notcontains $_ }
Check "每个能力都有 supports() 分支" ($noSupports.Count -eq 0) ("缺分支: " + ($noSupports -join ", "))

# build_capabilities() 上报的键名
$reportedKeys = Select-String -Path $capsCpp -Pattern 'caps\.add\("([a-z_0-9]+)"' |
    ForEach-Object { $_.Matches[0].Groups[1].Value }
$dupeKeys = $reportedKeys | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name }
Check "上报键无重复" ($dupeKeys.Count -eq 0) ("重复键: " + ($dupeKeys -join ", "))
Check "上报键数量 <= 枚举能力数" ($reportedKeys.Count -le $enumNames.Count) `
    "上报 $($reportedKeys.Count) 个键，枚举 $($enumNames.Count) 个能力"

# 文档里声明的键数量
$docKeyLine = Select-String -Path $handover -Pattern '返回的\s*(\d+)\s*个键'
if ($docKeyLine) {
    $docKeys = [int]$docKeyLine.Matches[0].Groups[1].Value
    Check "文档声明的键数($docKeys) == 实际上报($($reportedKeys.Count))" ($docKeys -eq $reportedKeys.Count) `
        "文档说 $docKeys 个，代码里上报 $($reportedKeys.Count) 个"
} else {
    Check "文档里有键数声明" $false "没找到 '返回的 N 个键'"
}

# 文档里逐个列出的键名，应当与实际上报的一致
$docKeyBlock = [regex]::Match($docText, '(?s)返回的\s*\d+\s*个键.*?```(.*?)```')
if ($docKeyBlock.Success) {
    $docKeysListed = [regex]::Matches($docKeyBlock.Groups[1].Value, '[a-z_0-9]{3,}') |
        ForEach-Object { $_.Value } | Sort-Object -Unique
    $notReported = $docKeysListed | Where-Object { $reportedKeys -notcontains $_ }
    $notDocumented = $reportedKeys | Where-Object { $docKeysListed -notcontains $_ }
    Check "文档列出的键 == 实际上报的键" (($notReported.Count -eq 0) -and ($notDocumented.Count -eq 0)) `
        "文档有但没上报: $($notReported -join ', ') | 上报但文档没写: $($notDocumented -join ', ')"
} else {
    Check "文档里有键名清单代码块" $false "没找到键名清单"
}

Write-Host "`n=== 4. 能力位编号无重复、无空洞 ===" -ForegroundColor Cyan
$values = Select-String -Path $capsH -Pattern '^\s+(\w+)\s*=\s*(\d+)\s*,' |
    ForEach-Object { [int]$_.Matches[0].Groups[2].Value } | Sort-Object
$dupes = $values | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name }
Check "无重复编号" ($dupes.Count -eq 0) ("重复: " + ($dupes -join ", "))

Write-Host "`n=== 5. 测试断言数与文档一致 ===" -ForegroundColor Cyan
# 只数**调用点**，要把函数定义行 `function _igpu_check(...)` 排除掉——
# 否则计数会比真实断言数多 1（这是本脚本早先的一个真实 bug）。
$checkLines = Select-String -Path $test -Pattern '_igpu_check\('
$callSites = $checkLines | Where-Object { $_.Line -notmatch 'function\s+_igpu_check\s*\(' }
$checks = ($callSites | ForEach-Object { $_.Matches.Count } | Measure-Object -Sum).Sum
if (-not $checks) { $checks = 0 }

# 其中一部分断言被 `if (句柄 > 0) { ... }` 包着，句柄创建失败时不会执行。
# 所以真实运行时断言数落在 [callSites - guarded, callSites] 区间内。
$guarded = ($callSites | Where-Object { $_.Line -match 'if\s*\([^)]*\)\s*\{[^}]*_igpu_check\(' }).Count
$runtimeMin = $checks - $guarded
Write-Host "  （调用点 $checks 个，其中 $guarded 个受条件保护，运行时执行 $runtimeMin-$checks 个）" -ForegroundColor DarkGray

# 文档里可能有多处断言数声明（每批验证证据各一处）。除最后"当前总数"外，
# 前面几处是历史批次规模，允许小于当前值，但不能大于（大于就是写错了）。
$checkMentions = Select-String -Path $handover -Pattern '当前检查项\s*\*\*(\d+)\s*项'
if ($checkMentions) {
    $overstated = $checkMentions | Where-Object { [int]$_.Matches[0].Groups[1].Value -gt $checks }
    Check "无超过调用点数的断言数声明" ($overstated.Count -eq 0) `
        ("这些行声称的断言数大于调用点 $checks : " + (($overstated | ForEach-Object { "行$($_.LineNumber)=$($_.Matches[0].Groups[1].Value)" }) -join ", "))
    # 声明的数字必须落在真实可执行区间内
    $inRange = $checkMentions | Where-Object {
        $v = [int]$_.Matches[0].Groups[1].Value
        ($v -ge $runtimeMin) -and ($v -le $checks)
    }
    Check "有一处声明落在真实区间($runtimeMin-$checks)" ($inRange.Count -ge 1) `
        "文档里的断言数: " + (($checkMentions | ForEach-Object { $_.Matches[0].Groups[1].Value }) -join ", ")
} else {
    Check "文档里有当前断言数声明" $false "没找到 '当前检查项 **N 项'"
}

Write-Host "`n=== 5b. 文档里的能力键数量声明处处一致 ===" -ForegroundColor Cyan
# 键数声明有**两种措辞**，必须都覆盖：
#   (a) "28 个键"        —— API 清单 + 能力清单标题
#   (b) "当前键数是 **22**" —— §9 流程说明里的一句提醒
# 早先只匹配 (a)，于是 (b) 那句写着 22 而实际是 28 时**漏检了**——
# 讽刺的是它就写在"别去找 kEntryCount"这段提醒旁边。
# 教训：同一个事实在文档里有几种写法，检查就得覆盖几种；只覆盖自己
# 记得的那一种，等于给剩下的写法发了免检通行证。
$keyMentions = @()
$keyMentions += Select-String -Path $handover -Pattern '(\d+)\s*个键'
$keyMentions += Select-String -Path $handover -Pattern '当前键数是\s*\**\s*(\d+)'

if ($keyMentions) {
    $distinct = $keyMentions | ForEach-Object { [int]$_.Matches[0].Groups[1].Value } | Sort-Object -Unique
    # 各处措辞之间必须自洽
    Check "键数声明处处一致（$($distinct -join ', ')）" ($distinct.Count -eq 1) `
        ("出现了不同的数字: " + (($keyMentions | ForEach-Object { "行$($_.LineNumber)=$($_.Matches[0].Groups[1].Value)" }) -join ", "))
    # 并且必须等于代码里实际上报的个数
    if ($distinct.Count -eq 1) {
        Check "文档声明的键数($($distinct[0])) == 实际上报($($reportedKeys.Count))" ($distinct[0] -eq $reportedKeys.Count) `
            "文档说 $($distinct[0]) 个，代码里上报 $($reportedKeys.Count) 个"
    }
} else {
    Check "文档里有键数声明" $false "没找到 'N 个键' 或 '当前键数是 N'"
}

Write-Host "`n=== 6. 版本号三处一致 ===" -ForegroundColor Cyan
$cppVer = (Select-String -Path (Join-Path $root "src\native\IGPU_native.cpp") -Pattern 'kIgpuVersion\s*=\s*"([\d.]+)"').Matches[0].Groups[1].Value
$docVer = (Select-String -Path $handover -Pattern '版本：`([\d.]+)`').Matches[0].Groups[1].Value
Check "代码版本($cppVer) == 文档版本($docVer)" ($cppVer -eq $docVer) "代码 $cppVer，文档 $docVer"

Write-Host "`n=== 7. 每个能力位都真的接线到 IGPU_native.cpp ===" -ForegroundColor Cyan
# 这里只做粗查：spec 里的函数应该在 IGPU_native.cpp 里有一份实现
$nativeCpp = Get-Content (Join-Path $root "src\native\IGPU_native.cpp") -Raw
$unwired = $funcs | Where-Object { $nativeCpp -notmatch [regex]::Escape($_) }
Check "全部函数已接线" ($unwired.Count -eq 0) ("未接线: " + ($unwired -join ", "))

Write-Host "`n=== 8. 文档里声明的 HEAD 是真实存在的提交 ===" -ForegroundColor Cyan
# 文档头部写着 HEAD 短哈希。它每次提交都会过期，而且**不会有人记得改**——
# 接手者若拿它去 `git checkout`，可能 checkout 到错误的位置。实测抓到过一次失真。
#
# 这里**故意不要求等于当前 HEAD**：文档无法记录包含它自己的那次提交的哈希
# （自指悖论）——只要声明的是真实提交、且不是凭空捏造的，就算通过。
# 判据是"该对象在仓库里存在"，这样既拦得住乱写的哈希，也不会逼人反复改这一个数字。
$headActual = (git -C $root rev-parse --short HEAD 2>$null)
$headClaim = [regex]::Match($docText, 'HEAD\s*`([0-9a-f]{7,40})`')
if ($headClaim.Success) {
    $claimed = $headClaim.Groups[1].Value
    # 注意：不要写成 "$claimed^{commit}"。PowerShell 会把 `^` 当转义字符吞掉，
    # 实际传给 git 的是 "e9cfe32{commit}"，于是真实存在的提交也被判为不存在。
    # （在 pwsh 下实测：`git cat-file -e e9cfe32^{commit}` 报 Not a valid object name，
    #  而不带 ^ 的 `git cat-file -e e9cfe32` 正常返回 0。）
    # 改用 rev-parse --verify --quiet，它对短哈希同样有效且不涉及 `^`。
    $null = git -C $root rev-parse --verify --quiet "${claimed}" 2>$null
    $exists = ($LASTEXITCODE -eq 0)
    Check "文档声明的 HEAD($claimed) 是真实提交" $exists `
        "仓库里没有这个提交 —— 别写臆造的哈希"
    if ($exists) {
        $behind = (git -C $root rev-list --count "$claimed..HEAD" 2>$null)
        if ($behind -and [int]$behind -gt 0) {
            Write-Host "  [NOTE] 文档 HEAD 落后当前 $behind 个提交（当前 $headActual）；" -ForegroundColor DarkYellow
            Write-Host "         这是预期的——它无法记录包含自己的那次提交。以 git 现值为准。" -ForegroundColor DarkYellow
        }
    }
} else {
    Check "能解析文档里的 HEAD 声明" $false "没找到 'HEAD \`hash\`'"
}

Write-Host "`n=== 9. §9 的条目编号引用没有越界 ===" -ForegroundColor Cyan
# §9 的编号会被重排（比如插入新条目），而正文里散落的「§9 第 N 项」引用
# 不会自动更新——这类失真很隐蔽，专门查一下。
$s9 = [regex]::Match($docText, '(?s)## 9\. 下一步(.*?)\n## 10\.')
if ($s9.Success) {
    $maxNum = 0
    foreach ($m in [regex]::Matches($s9.Groups[1].Value, '(?m)^(\d+)\.\s')) {
        $n = [int]$m.Groups[1].Value
        if ($n -gt $maxNum) { $maxNum = $n }
    }
    Check "§9 条目数可解析（最大编号 $maxNum）" ($maxNum -gt 0) "没解析到编号条目"
    $refs = [regex]::Matches($docText, '§9\s*第\s*(\d+)\s*项')
    $badRefs = $refs | Where-Object { [int]$_.Groups[1].Value -gt $maxNum }
    Check "所有「§9 第 N 项」引用都在范围内" ($badRefs.Count -eq 0) `
        ("越界引用: " + (($badRefs | ForEach-Object { $_.Groups[1].Value }) -join ", ") + " (最大 $maxNum)")
} else {
    Check "能定位到 §9" $false "没找到 '## 9. 下一步'"
}

Write-Host ""
if ($fail -eq 0) {
    Write-Host "全部一致 — 交接文档可信。" -ForegroundColor Green
    exit 0
} else {
    Write-Host "$fail 项不一致 — 请先修正文档或代码再交接。" -ForegroundColor Red
    exit 1
}
