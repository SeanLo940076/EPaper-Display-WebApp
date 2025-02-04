#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <cstdint>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

uint8_t palette[4 * 3] = {
	0, 0, 0,
	255, 255, 255,
	255, 255, 0,
	255, 0, 0,
};

int depalette(uint8_t* color)
{
	int p;
	int mindiff = 100000000;
	int bestc = 0;
	for (p = 0; p < sizeof(palette) / 3; p++)
	{
		int diffr = ((int)color[0]) - ((int)palette[p * 3 + 0]);
		int diffg = ((int)color[1]) - ((int)palette[p * 3 + 1]);
		int diffb = ((int)color[2]) - ((int)palette[p * 3 + 2]);
		int diff = (diffr * diffr) + (diffg * diffg) + (diffb * diffb);
		if (diff < mindiff)
		{
			mindiff = diff;
			bestc = p;
		}
	}
	return bestc;
}

int main(int argc, char** argv)
{

	if (argc < 2)
	{
		fprintf(stderr, "Usage: converter [image file] [out file]\n");
		printf("Press Enter to exit.");
		getchar();
		return -1;
	}

	int x, y, n;
	unsigned char* data = stbi_load(argv[1], &x, &y, &n, 0);
	if (!data)
	{
		fprintf(stderr, "Error: Can't open image.\n");
		printf("Press Enter to exit.");
		getchar();
		return -6;
	}

	int i, j, Width, Height;
	int  k = 0, flag = 0;
	int c[4] = {0x00};
	errno_t err = 0;
	FILE* fout = NULL;

	if (argc == 2)
		err = fopen_s(&fout, "c4_image.c", "wb");
	else
		err = fopen_s(&fout, argv[2], "wb");
	 
	int num = 0;

	fprintf(fout, "// 4 Color Image Data %d*%d \r\n", x, y);

	Height = y;
	Width = (x % 4 == 0) ? (x / 4) : (x / 4 + 1);
	fprintf(fout, "const unsigned char Image4color[%d] = {\r\n", Width * Height);

	flag = (x % 4 == 0) ? 0 : 1;

	for (j = 0; j < Height; j++)
	{
		for (i = 0; i < Width; i++)
		{
			if((flag) && i == Width-1)
			{
				c[0] = 0;
				c[1] = 0;
				c[2] = 0;
				c[3] = 0;
				for (k = 0; k < x % 4; k++)
				{
					c[k] = depalette(data + n * (i * 4 + x * j + k));
				}
			}
			else
			{
				c[0] = depalette(data + n * (i * 4 + x * j));
				c[1] = depalette(data + n * (i * 4 + x * j + 1));
				c[2] = depalette(data + n * (i * 4 + x * j + 2));
				c[3] = depalette(data + n * (i * 4 + x * j + 3));
			}
				
			unsigned char uc = c[3] | (c[2] << 2) | (c[1] << 4) | (c[0] << 6);

			fprintf(fout, "0x%02X,", uc);
			if (num++ == 15)
			{
				fprintf(fout, "\r\n");
				num = 0;
			}
		}
	}
	fprintf(fout, "};\r\n");
	fclose(fout);
	stbi_image_free(data);
	return 0;
}
