for i in $(find ../regression -name "*.bc");
do
  filename=$(basename "$i" .bc)
  ./hw2 "../regression/$filename.bc" "../regression/$filename.lama"
done