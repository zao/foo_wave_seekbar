@echo off
for %%a in (%*) do (
	signtool sign ^
	/a ^
	/t http://timestamp.verisign.com/scripts/timstamp.dll ^
	/i "Certum Level III CA" ^
	/d "%%a" ^
	"%%a"
)
