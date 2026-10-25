# Usage: powershell -File to_h264.ps1 -In <file.mp4> -Out <file.mp4> [-Kbps 6000]
# Re-encodes a video to H.264 MP4 with the Media Foundation encoder built into Windows (WinRT MediaTranscoder).
param([string]$In, [string]$Out, [int]$Kbps = 6000)
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$asTask = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
$asTaskProgress = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncActionWithProgress`1' })[0]
function Await($op, [Type]$type) {
    $task = $asTask.MakeGenericMethod($type).Invoke($null, @($op)); $task.Wait(-1) | Out-Null; $task.Result
}
[Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime] | Out-Null
[Windows.Media.Transcoding.MediaTranscoder, Windows.Media.Transcoding, ContentType = WindowsRuntime] | Out-Null
[Windows.Media.MediaProperties.MediaEncodingProfile, Windows.Media.MediaProperties, ContentType = WindowsRuntime] | Out-Null

$src = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync((Resolve-Path $In).Path)) ([Windows.Storage.StorageFile])
$dir = Await ([Windows.Storage.StorageFolder]::GetFolderFromPathAsync((Split-Path -Parent ([IO.Path]::GetFullPath($Out))))) ([Windows.Storage.StorageFolder])
$dst = Await ($dir.CreateFileAsync((Split-Path -Leaf $Out), [Windows.Storage.CreationCollisionOption]::ReplaceExisting)) ([Windows.Storage.StorageFile])

$profile = [Windows.Media.MediaProperties.MediaEncodingProfile]::CreateMp4([Windows.Media.MediaProperties.VideoEncodingQuality]::HD720p)
$profile.Video.Bitrate = [uint32]($Kbps * 1000)
$profile.Audio = $null
$transcoder = New-Object Windows.Media.Transcoding.MediaTranscoder
$prepared = Await ($transcoder.PrepareFileTranscodeAsync($src, $dst, $profile)) ([Windows.Media.Transcoding.PrepareTranscodeResult])
if (-not $prepared.CanTranscode) { Write-Output "cannot transcode: $($prepared.FailureReason)"; exit 1 }
$task = $asTaskProgress.MakeGenericMethod([double]).Invoke($null, @($prepared.TranscodeAsync()))
$task.Wait(-1) | Out-Null
Write-Output "done $((Get-Item $Out).Length / 1MB) MB"
