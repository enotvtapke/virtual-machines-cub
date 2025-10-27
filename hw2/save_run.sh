for i in $(find ./regression -name "test*.bc" | sort);
do
  filename=$(basename "$i" .bc)
  ./cmake-build-debug/hw2 "./regression/$filename.bc" "./regression/$filename.lama"
done