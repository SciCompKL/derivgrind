path_of_config_script=$(readlink -f $0) 
bindir=$(dirname $path_of_config_script)
installdir=$(dirname $bindir)

incdir=$installdir/include
libdir=$installdir/lib
pythondir=$installdir/lib/python3/site-packages

usage="\
Usage: derivgrind-config [--cflags] [--libs] [--incdir]"

if test $# -eq 0; then
  echo "${usage}" 1>&2
  exit 1
fi

out=""

while test $# -gt 0; do
  case "$1" in
    --cflags) out="$out -I$incdir" ;;
    --incdir) out="$out $incdir" ;;
    --bindir) out="$out $bindir" ;;
    --installdir) out="$out $installdir" ;;
    --pythondir) out="$out $pythondir" ;;
  esac
  shift
done

echo $out


