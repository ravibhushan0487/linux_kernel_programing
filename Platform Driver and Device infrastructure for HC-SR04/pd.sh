cd /sys/class/HCSR/HCSR_0/
echo "8" >trigger_pin
echo "9" >echo_pin
cat trigger_pin
cat echo_pin

cd /sys/class/HCSR/HCSR_0/
echo "300">no_of_samples
echo "5">delta
#echo "0">enable
cat no_of_samples
cat delta
cat enable
cat trigger_pin
cat echo_pin
