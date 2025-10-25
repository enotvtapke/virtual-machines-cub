for i in $(find . -name "*.lama");
do
    lamac -ds $i
done