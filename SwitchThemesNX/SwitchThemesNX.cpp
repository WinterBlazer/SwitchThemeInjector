#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include "SwitchThemesCommon.hpp"

using namespace std;

vector<u8> OpenFile(const string &name)
{
	basic_ifstream<u8> fin{ name, ios::binary };
	if (fin.fail())
		throw "File open failed";

	fin.seekg(0, fin.end);
	size_t len = fin.tellg();
	fin.seekg(0, fin.beg);

	vector<u8> coll(len);
	fin.read(coll.data(), len);
	fin.close();
	return coll;
}

string OpenTextFile(const string &name) 
{
	ifstream t(name);
	if (t.fail())
		throw "File open failed";

	std::string str;

	t.seekg(0, std::ios::end);
	str.reserve(t.tellg());
	t.seekg(0, std::ios::beg);

	str.assign((std::istreambuf_iterator<char>(t)),
		std::istreambuf_iterator<char>());
	
	return str;
}

void WriteFile(const string &name,const vector<u8> &data)
{
	basic_ofstream<u8> fout{ name, ios::binary };
	if (fout.fail())
		throw "File open failed";
	fout.write(data.data(), data.size());
	fout.close();
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		cout << "Usage:\nSwitchThemesNX <Theme pack> <SZS File>" << endl;
		return 1;
	}

	string TargetPath(argv[1]);
	vector<u8> ThemePack = OpenFile(TargetPath);
	ThemePack = Yaz0::Decompress(ThemePack);
	auto ThemeData = SARC::Unpack(ThemePack);

	if (!ThemeData.files.count("image.dds"))
	{
		cout << "This theme pack is not valid" << endl;
		return 1;
	}

	string SZSPath(argv[2]);

	vector<u8> SZS = OpenFile(SZSPath);
	SZS = Yaz0::Decompress(SZS);
	auto SData = SARC::Unpack(SZS);

	cout << "Opened SARC: " << endl
		<< "DecSize: " << SZS.size() << endl
		<< "Endianness: " << to_string((u8)SData.endianness) << endl
		<< "Hash only: " << to_string(SData.HashOnly) << endl
		<< "Files : " << endl;

	for (auto f : SData.files)
	{
		cout << f.first << " : " << f.second.size() << " Bytes" << endl;
	}

	cout << "\nDetecting sarc..." << endl;
	auto patch = SwitchThemesCommon::DetectSarc(SData);
	if (patch.FirmName == "")
	{
		cout << "No compatible patch template found." << endl;
		return 2;
	}
	cout << "Found target: " << patch.TemplateName << " for " << patch.FirmName << " [" << patch.szsName << "]" << endl;

	cout << "Patching BG layout.....";
	auto pResult = SwitchThemesCommon::PatchBgLayouts(SData, patch);
	if (pResult != BflytFile::PatchResult::OK)
	{
		cout << "SwitchThemesCommon::PatchBgLayouts != BflytFile::PatchResult::OK." << endl;
		return 3;
	}
	cout << "OK" << endl;

	cout << "Injecting DDS......";
	pResult = SwitchThemesCommon::PatchBntx(SData, ThemeData.files["image.dds"], patch);
	if (pResult != BflytFile::PatchResult::OK)
	{
		cout << "SwitchThemesCommon::PatchBntx != BflytFile::PatchResult::OK." << endl;
		return 4;
	}
	cout << "OK" << endl;

	if (ThemeData.files.count("layout.json"))
	{
		cout << "Patching layout" << endl;
		auto JsonBinary = ThemeData.files["layout.json"];
		string JSON(reinterpret_cast<char*>(JsonBinary.data()), JsonBinary.size());
		auto patch = Patches::LoadLayout(JSON);
		if (patch.Files.size() == 0) {
			cout << "Invalid layout patch file" << endl;
			return 5;
		}
		cout << "Using patch " << patch.PatchName << " by " << patch.AuthorName << endl;
		cout << "Checking compatibility.....";
		if (!patch.IsCompatible(SData))
		{
			cout << "FAIL\nThe selected patch is not compatible with this SZS" << endl;
			return 6;
		}
		cout << "OK" << endl;
		auto res = SwitchThemesCommon::PatchLayouts(SData, patch.Files);
		if (res != BflytFile::PatchResult::OK)
		{
			cout << "SwitchThemesCommon::PatchLayouts != BflytFile::PatchResult::OK." << endl;
			return 7;
		}
		cout << "Patch applied" << endl;
	}

	cout << "Repacking sarc..." << endl;
	auto packed = SARC::Pack(SData);
	cout << "compressing..." << endl;
	auto compressed = Yaz0::Compress(packed.data, 3, packed.align);
	WriteFile(  patch.szsName, compressed);

	cout << "DONE !" << endl;

	getchar();
    return 0;
}

