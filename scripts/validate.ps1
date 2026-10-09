$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$preferred = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python313\python.exe'
$python = if (Test-Path -LiteralPath $preferred) { $preferred } else { (Get-Command python -ErrorAction Stop).Source }

& $python (Join-Path $root 'scripts\generate_keymap.py') --check
if ($LASTEXITCODE) { throw 'The generated WIN68 keymap is out of date.' }
& $python (Join-Path $root 'scripts\check_layout.py')
if ($LASTEXITCODE) { throw 'WIN68 native layout validation failed.' }

$vendor = Join-Path $root '..\Aether-HE-main\ui\layouts\aula-win68he-si2828heargb.json'
$bundled = Join-Path $root 'layout\aula-win68he-si2828heargb.json'
if (Test-Path -LiteralPath $vendor) {
    if ((Get-FileHash -LiteralPath $vendor -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $bundled -Algorithm SHA256).Hash) {
        throw 'Bundled vendor layout differs from the sibling Aether-HE layout.'
    }
}
