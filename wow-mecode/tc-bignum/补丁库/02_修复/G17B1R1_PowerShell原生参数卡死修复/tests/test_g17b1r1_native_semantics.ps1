$ErrorActionPreference='Stop'
$path=Join-Path (Split-Path $PSScriptRoot -Parent) 'payload/Install-Build-G17B1R1-Windows.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'AST errors'}
foreach($name in @('W','Invoke-NativeLogged')){
 $f=$ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true)
 if($f.Count -ne 1){throw "$name count=$($f.Count)"};Invoke-Expression $f[0].Extent.Text
}
$Result='/tmp/g17b1r1-native.log';$Utf8NoBom=New-Object System.Text.UTF8Encoding($false);[IO.File]::WriteAllText($Result,'',$Utf8NoBom)
$argsOk=@('-c','printf ''OUT:%s'' "$1"; printf ''ERR:%s'' "$2" >&2','_','alpha beta','gamma"delta')
$rc=Invoke-NativeLogged -FilePath '/bin/bash' -NativeArgs $argsOk -Prefix 'SEMANTIC_OK'
if($rc -ne 0){throw "ok rc=$rc"}
$text=Get-Content -Raw $Result
if($text -notmatch 'SEMANTIC_OK\|OUT:alpha beta'){throw 'stdout/space argument not preserved'}
if($text -notmatch 'SEMANTIC_OK\|ERR:gamma"delta'){throw 'stderr/quote argument not preserved'}
$rc=Invoke-NativeLogged -FilePath '/bin/bash' -NativeArgs @('-c','printf ''FAILOUT''; printf ''FAILERR'' >&2; exit 37') -Prefix 'SEMANTIC_FAIL'
if($rc -ne 37){throw "failed rc=$rc"}
$text=Get-Content -Raw $Result
if($text -notmatch 'SEMANTIC_FAIL\|FAILOUT' -or $text -notmatch 'SEMANTIC_FAIL\|FAILERR'){throw 'failed process streams not logged'}
'G17B1R1_REAL_NATIVE_STDOUT_STDERR_EXIT_TEST=PASS'
