OUT = am335x-boneblack.dtb
SOURCE = am335x.dts

all:
	dtc -I dts -O dtb -o $(OUT) $(SOURCE)
clean:
	rm $(OUT)

upload: all
	scp $(SOURCE) $(OUT) ubuntu@192.168.0.14:/home/ubuntu/dts/