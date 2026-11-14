$ErrorActionPreference='Stop'
. "$PSScriptRoot/Compare-Decodes.ps1"
$fixtures=@(@{id='one'})
function Reports($before,$after) {
    @(@{events=@(@{event='result';caseId='one';rows=@($before|ForEach-Object {@{message=$_}})})},
      @{events=@(@{event='result';caseId='one';rows=@($after|ForEach-Object {@{message=$_}})})},
      @{events=@(@{event='result';caseId='one';rows=@($after|ForEach-Object {@{message=$_}})})},
      @{events=@(@{event='result';caseId='one';rows=@($before|ForEach-Object {@{message=$_}})})})
}
$same=@(Compare-Decodes -Reports (Reports @('CQ K1ABC FN42','CQ W1XYZ FN31') @('CQ W1XYZ FN31','CQ K1ABC FN42')) -Fixtures $fixtures)
if($same.Count -ne 2 -or @($same|Where-Object {!$_.identical}).Count) { throw 'Equal reordered messages failed' }
$changed=@(Compare-Decodes -Reports (Reports @('CQ K1ABC FN42') @('CQ W1XYZ FN31')) -Fixtures $fixtures)
if(@($changed|Where-Object {$_.identical}).Count) { throw 'Same count with different message accepted' }
$missing=@(Compare-Decodes -Reports (Reports @('CQ K1ABC FN42','CQ K1ABC FN42') @('CQ K1ABC FN42')) -Fixtures $fixtures)
if(@($missing|Where-Object {$_.identical}).Count) { throw 'Missing duplicate accepted' }
$empty=@(Compare-Decodes -Reports (Reports @() @()) -Fixtures $fixtures)
if(@($empty|Where-Object {!$_.identical}).Count) { throw 'Equal empty messages failed' }
'Comparator: four positive/negative cases passed in both A/B pairs'
