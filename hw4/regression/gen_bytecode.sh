for i in $(find . -name "*.lama");
do
    lamac -b $i
done