# mueslee 2014
echo "[x] create tmp dir and backup"
mkdir tmp
cp "$1" tmp
cd tmp
echo "[x] converting pdf to jpg"
pdfimages -j "$1" ./image
base=$(basename "$1" .pdf)
echo "[x] delete pdf"
rm "$1"
echo "[x] crunching"
rar a "$base" * >/dev/null 2>&1
mv "$base".rar "$base".cbr
mv "$base".cbr ..
rm image*.jpg
cd ..
rmdir tmp
echo "[x] done"
