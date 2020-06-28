#
# morse_shield_nrt
#
ver=20200622a

# archive name
target=morse-shield_nrt-$(ver)

# base file to make diff
base=../morse-shield_191220/morse-shield_191220.ino

dist:
	diff -u $(base) morse-shield_nrt.ino > arduino_morse_runner_$(ver).diff ; true
	tar cvf $(target).tar \
		README_morse-shield_nrt.txt \
		Makefile \
		morse-shield_nrt.ino \
		arduino_morse_runner_$(ver).diff
	gzip -f $(target).tar
	ls -alF $(target).tar.gz
