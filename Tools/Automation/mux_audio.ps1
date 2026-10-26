# Usage: powershell -File mux_audio.ps1 -Video <in.mp4> -Audio <music.wav> -Out <out.mp4> [-Kbps 3500]
# Puts a music track under a video with Windows' own MediaComposition and renders H.264 + AAC MP4.
param([string]$Video, [string]$Audio, [string]$Out, [int]$Kbps = 3500, [int]$Width = 0, [int]$Height = 0)
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$methods = [System.WindowsRuntimeSystemExtensions].GetMethods()
$asTask = ($methods | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
$asTaskProgress = ($methods | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperationWithProgress`2' })[0]
function Await($op, [Type]$type) { $t = $asTask.MakeGenericMethod($type).Invoke($null, @($op)); $t.Wait(-1) | Out-Null; $t.Result }
[Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime] | Out-Null
[Windows.Media.Editing.MediaComposition, Windows.Media.Editing, ContentType = WindowsRuntime] | Out-Null
[Windows.Media.MediaProperties.MediaEncodingProfile, Windows.Media.MediaProperties, ContentType = WindowsRuntime] | Out-Null

$videoFile = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync((Resolve-Path $Video).Path)) ([Windows.Storage.StorageFile])
$audioFile = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync((Resolve-Path $Audio).Path)) ([Windows.Storage.StorageFile])
$dir = Await ([Windows.Storage.StorageFolder]::GetFolderFromPathAsync((Split-Path -Parent ([IO.Path]::GetFullPath($Out))))) ([Windows.Storage.StorageFolder])
$dst = Await ($dir.CreateFileAsync((Split-Path -Leaf $Out), [Windows.Storage.CreationCollisionOption]::ReplaceExisting)) ([Windows.Storage.StorageFile])

$composition = New-Object Windows.Media.Editing.MediaComposition
$clip = Await ([Windows.Media.Editing.MediaClip]::CreateFromFileAsync($videoFile)) ([Windows.Media.Editing.MediaClip])
[System.Collections.Generic.ICollection[Windows.Media.Editing.MediaClip]].GetMethod("Add").Invoke($composition.Clips, @($clip))
$track = Await ([Windows.Media.Editing.BackgroundAudioTrack]::CreateFromFileAsync($audioFile)) ([Windows.Media.Editing.BackgroundAudioTrack])
[System.Collections.Generic.ICollection[Windows.Media.Editing.BackgroundAudioTrack]].GetMethod("Add").Invoke($composition.BackgroundAudioTracks, @($track))

$profile = [Windows.Media.MediaProperties.MediaEncodingProfile]::CreateMp4([Windows.Media.MediaProperties.VideoEncodingQuality]::HD720p)
$profile.Video.Bitrate = [uint32]($Kbps * 1000)
if ($Width -gt 0) { $profile.Video.Width = [uint32]$Width; $profile.Video.Height = [uint32]$Height }
$op = $composition.RenderToFileAsync($dst, [Windows.Media.Editing.MediaTrimmingPreference]::Precise, $profile)
$task = $asTaskProgress.MakeGenericMethod([Windows.Media.Transcoding.TranscodeFailureReason], [double]).Invoke($null, @($op))
$task.Wait(-1) | Out-Null
Write-Output "result $($task.Result) size $((Get-Item $Out).Length / 1MB) MB"
