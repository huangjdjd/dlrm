#include <stdio.h>
#include <liblightnvm.h>
#include <iostream>
#include <string>
#include <math.h>
using namespace std;

int main(int argc, char **argv)
{
	const struct nvm_dev *dev;
	const struct nvm_geo *geo;
	dev = nvm_dev_open("/dev/nvme0n1");
	geo = nvm_dev_get_geo(dev);
	size_t ws_opt = nvm_dev_get_ws_opt(dev);
	printf("%lu",ws_opt);
}
