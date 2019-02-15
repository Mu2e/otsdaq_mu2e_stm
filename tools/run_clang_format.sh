
if [ ! -d ${CLANG_FQ_DIR:-""} ]; then
    echo "You must first setup Clang! (setup clang v5_0_1)"
    exit 1
fi
clang-format -i `find . -type f -name *.icc -o -name *.cc -o -name *.c -o -name *.cpp -o -name *.cxx -o -name *.h -o -name *.hh -o -name *.hxx -o -name *.hpp`

