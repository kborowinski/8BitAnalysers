echo off

echo Deleting old package files...
if exist ..\Package rmdir /s /q ..\Package

mkdir ..\Package\Loose\Configs
mkdir ..\Package\Loose\GameData
mkdir ..\Package\Loose\Games
mkdir ..\Package\Loose\Games128
mkdir ..\Package\Loose\Pokes
mkdir ..\Package\Loose\RZX
mkdir ..\Package\Loose\Docs
mkdir ..\Package\Loose\Fonts

rem Add some files to describe what to put in the empty directories
@echo>"..\Package\Loose\Games\48k snapshots go here - z80 or sna"
@echo>"..\Package\Loose\Games128\128k snapshots go here - z80 or sna"
@echo>"..\Package\Loose\Pokes\pok files go here - filename matching game name"
@echo>"..\Package\Loose\RZX\rzx files go here"
@echo>"..\Package\Loose\Fonts\fonts go here in ttf format"

copy ..\Source\ZXSpectrum\build\bin\Release\SpectrumAnalyser.exe ..\Package\Loose\
copy ..\Data\SpectrumAnalyser\imgui.ini ..\Package\Loose\
copy ..\Data\SpectrumAnalyser\RomInfo.json ..\Package\Loose\
copy ..\Data\SpectrumAnalyser\SALogo.png ..\Package\Loose\
copy ..\Docs\*.* ..\Package\Loose\Docs\

rem Make a batch file to run the 128k version
@echo SpectrumAnalyser.exe -128> "..\Package\Loose\SpectrumAnalyser128k.bat"

7z a ..\Package\SpectrumAnalyser.zip ..\Package\Loose\.

pause