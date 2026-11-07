@echo off
rem Runs the ASan client next to this file. Each unique error is logged once to asan.log and the game keeps running.
rem The empty external_symbolizer_path keeps ASan off any llvm-symbolizer on PATH, which hangs on modules without a PDB.
setlocal
set "ASAN_OPTIONS=continue_on_error=2:external_symbolizer_path="
"%~dp0WowClient64.exe" %* 2> "%~dp0asan.log"
