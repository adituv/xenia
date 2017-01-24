#include <fstream>
#include <iterator>
#include "xenia/gpu/gl4/texture_cache.h"

using namespace std;
using namespace xe::gpu;
using namespace xe::gpu::gl4;

struct ImgHeader {
	union {
		struct {
			uint32_t unk1;
			uint32_t unk2;
			uint16_t width1;
			uint16_t height1;
			uint16_t depth1;
			uint16_t width2;
			uint16_t height2;
			uint16_t depth2;
			uint8_t mipmaps;
			uint8_t formatType;
		};
		char raw[0x60];
	};
};

template<typename T>
T SwapEndian(T src) {
	size_t s = sizeof(T);
	char* bytes = reinterpret_cast<char*>(&src);
	int i = 0, j = s - 1;
	while (i < j) {
		char tmp = bytes[i];
		bytes[i] = bytes[j];
		bytes[j] = tmp;
		i++; j--;
	}

	T result = *reinterpret_cast<T*>(bytes);
	return result;
}

void FixHeaderEndianness(ImgHeader& header) {
	header.unk1 = SwapEndian(header.unk1);
	header.unk2 = SwapEndian(header.unk2);
	header.width1 = SwapEndian(header.width1);
	header.height1 = SwapEndian(header.height1);
	header.depth1 = SwapEndian(header.depth1);
	header.width2 = SwapEndian(header.width2);
	header.height2 = SwapEndian(header.height2);
	header.depth2 = SwapEndian(header.depth2);
}

int main(int argc, const char* argv[]) {
	if (argc < 2) return 1;

	string filename = argv[1];

	ifstream in(filename, ifstream::in | ifstream::binary);
	auto header = make_unique<ImgHeader>();
	in.get(header->raw, 0x60);
	
	// get its size:
	int fileSize;

	in.seekg(0, std::ios::end);
	fileSize = in.tellg();
	in.seekg(0x1000, std::ios::beg);

	vector<uint8_t> data(fileSize - 0x1000);
	vector<uint8_t> out(fileSize - 0x1000, 0);
	data.insert(data.begin(), std::istream_iterator<uint8_t>(in), std::istream_iterator<uint8_t>());

	FixHeaderEndianness(*header);

	TextureInfo info;
	info.guest_address = 0; // Doesn't make sense in this context... hope it's not used? :S
	info.dimension = Dimension::k2D;
	info.width = header->width1;
	info.height = header->height1;
	info.depth = header->depth1;
	info.endianness = Endian::k8in16;
	info.is_tiled = true;
	info.input_length = fileSize - 0x1000;
	info.output_length = fileSize - 0x1000;
	
	info.size_2d.logical_width = header->width1;
	info.size_2d.logical_height = header->height1;
	info.size_2d.block_width = header->width2;
	info.size_2d.block_height = header->width2;
	info.size_2d.input_width = header->width1;
	info.size_2d.input_height = header->height1;
	info.size_2d.output_width = header->width1;
	info.size_2d.output_height = header->height1;

	auto fInfo = make_unique<FormatInfo>();
	switch (header->formatType) {
	case 1:
		fInfo->format = TextureFormat::k_DXT1;
		fInfo->bits_per_pixel = 4;
		break;
	case 2:
	case 3:
		fInfo->format = TextureFormat::k_DXT2_3;
		fInfo->bits_per_pixel = 8;
		break;
	case 4:
	case 5:
		fInfo->format = TextureFormat::k_DXT4_5;
		fInfo->bits_per_pixel = 8;
		break;
	default:
		return 1;
	}

	fInfo->type = FormatType::kCompressed;
	fInfo->block_width = 4;
	fInfo->block_height = 4;
	
	info.format_info = fInfo.get();

	UntileImage(info, data.data(), out.data());

	string outFileName = filename.append(".out");
	ofstream outFile(outFileName, ofstream::binary | ofstream::out);
	outFile.write(reinterpret_cast<char*>(out.data()), fileSize - 0x1000);
}