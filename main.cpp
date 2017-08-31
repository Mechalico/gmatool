#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <iterator>
//#include <unistd.h>
using namespace std;

/*

There are three parts to the goal tool:

1 - Extract goals, switches, or other models
2 - Merge any two GMAs and TPLs

*/

bool isLittleEndian()
//shamelessly pinched from StackOverflow
{
	short int number = 0x1;
	char *numPtr = (char*)&number;
	return (numPtr[0] == 1);
}

uint32_t fileIntPluck (ifstream& bif, uint32_t offset){
	bif.seekg(offset, bif.beg);
	char buffer[4]; //4 byte buffer
	bif.read(buffer, 0x4);
	//convert signed to unsigned
	unsigned char* ubuf = reinterpret_cast<unsigned char*>(buffer);
	uint32_t returnint = 0;
	if (isLittleEndian){
		returnint = (ubuf[3] << 0) | (ubuf[2] << 8) | (ubuf[1] << 16) | (ubuf[0] << 24);
	} else {
		returnint = (ubuf[0] << 0) | (ubuf[1] << 8) | (ubuf[2] << 16) | (ubuf[3] << 24);
	}
	return returnint;
}

uint16_t fileShortPluck (ifstream& bif, uint32_t offset){
	bif.seekg(offset, bif.beg);
	char buffer[2]; //2 byte buffer
	bif.read(buffer, 0x2);
	//convert signed to unsigned
	unsigned char* ubuf = reinterpret_cast<unsigned char*>(buffer);
	uint16_t returnshort = 0;
	if (isLittleEndian){
		returnshort = (ubuf[1] << 0) | (ubuf[0] << 8);
	} else {
		returnshort = (ubuf[0] << 0) | (ubuf[1] << 8);
	}
	return returnshort;
}

int helpText(){
	cout << "How to use gmatool:" << endl 
		<< "Each of these saves extracted data to unique and readable gma and tpl files, and do not alter the input files." << endl
		<< "\"-ge <name>\" - Extracts goal data from <name>.gma and <name>.tpl." << endl 
		<< "\"-se <name>\" - Extracts switch data from <name>.gma and <name>.tpl, saving each switch to unique files, including switch bases." << endl 
		<< "\"-me <name> <modelname>\" - Extracts the data of the model called \"modelname\" from <name>.gma and <name>.tpl." << endl
		<< "\"-m <name1> <name2>\" - Extracts all data from <name1>.gma, <name2>.gma, <name1>.tpl and <name2>.tpl, and combines the data. The second file's data is always placed after the first." << endl;
	return 1;
}

void copyBytes(ifstream& bif, ofstream& bof, uint64_t offset, uint64_t length){
	char bytes[length];
	bif.seekg(offset, bif.beg);
	bif.read(bytes, length);
	bof.write(bytes, length);
}

void saveIntToFileEnd(ofstream& bof, uint32_t newint){
	char buffer[4];
	char* initbuffer = reinterpret_cast<char*>(&newint);
		//assigns values wrt endianness
		if (isLittleEndian){
			for (int i = 0; i < 4; i++){
				buffer[i] = initbuffer[3-i];
			}
		} else {
			for (int i = 0; i < 4; i++){
				buffer[i] = initbuffer[i];
			}
		}
		//write float
		bof.write(buffer, sizeof(uint32_t));
}

void saveShortToFileEnd(ofstream& bof, uint16_t newint){
	char buffer[2];
	char* initbuffer = reinterpret_cast<char*>(&newint);
		//assigns values wrt endianness
		if (isLittleEndian){
			for (int i = 0; i < 2; i++){
				buffer[i] = initbuffer[1-i];
			}
		} else {
			for (int i = 0; i < 2; i++){
				buffer[i] = initbuffer[i];
			}
		}
		//write float
		bof.write(buffer, sizeof(uint16_t));
}

uint32_t getFileLength(ifstream& bif){
	bif.seekg(0, bif.end);
	uint32_t length = bif.tellg();
	return length;
}

uint32_t getModelNameLength(ifstream& bif, uint32_t modelnameoffset, uint32_t increment){
	uint32_t length = getFileLength(bif);
	bif.seekg(modelnameoffset, bif.beg);
	int modelnamelength = 0;
	bool endofmodelname = false;
	char endingbyte[1];
	while (endofmodelname == false){
		modelnamelength += increment;
		//in case it's beyond the length of the file
		if (modelnameoffset + modelnamelength > length){
			modelnamelength = length - modelnameoffset;
		}
		bif.seekg(modelnameoffset+modelnamelength-1, bif.beg);
		bif.read(endingbyte, 1);
		if (endingbyte[0] == 0){
			endofmodelname = true;
		}
	}
	return modelnamelength;
}


void padZeroes(ofstream& bof, uint32_t zeronumber){
	char buffer[1] = {0x0};
	for (int i = 0; i < zeronumber; i++){
		bof.write(buffer, 1);
	}
}

/*

Part 1:
Goal Extraction

*/

void modelWriteToFiles(string filename, ifstream& oldgma, ifstream& oldtpl, int modelamount, int modelnumber, uint32_t modelnamelength, string modelname, string suffix){
	/*
	These files will create standalone TPL and GMA files, designed to be easily integrated into the main file.
	*/
	//First delete files
	remove((filename + "_" + suffix + ".tpl").c_str());
	remove((filename + "_" + suffix + ".gma").c_str());
	//Write the GMA first, and we can get info for the TPL later
	ofstream newgma(filename + "_" + suffix + ".gma", ios::binary | ios::app);
	//Write the initial bytes
	saveIntToFileEnd(newgma, 1); //1 model
	//Calculate remaining length
	/*
	The GMA header is always a multiple of 0x20 in length. (modelnamelength + 0x10) % 0x20 gives the remaining padding
	*/
	uint32_t gmapadding = (-(modelnamelength+0x10)) % 0x20;
	uint32_t newheaderlength = modelnamelength+0x10+gmapadding;
	saveIntToFileEnd(newgma, newheaderlength);
	//Now for the zero bytes. These point to the extra offsets, of which there isn't one
	padZeroes(newgma, 8);
	//Now write in the modelname
	newgma << modelname;
	//pad to a multiple of 0x20
	padZeroes(newgma, gmapadding+1); //extra 1 due to missing 00 byte from modelname
	/*
	Now the header is written, time for the main body
	*/
	
	uint64_t oldheaderlength = fileIntPluck(oldgma, 0x04);
	uint64_t oldstartextraoffset = (fileIntPluck(oldgma, 0x08+(0x08*modelnumber)));
	uint64_t oldstartpoint = oldheaderlength + oldstartextraoffset;
	uint64_t oldendpoint = 0;
	if (modelamount == modelnumber+1){
		oldendpoint = getFileLength(oldgma);
	} else {
		uint64_t oldendextraoffset = fileIntPluck(oldgma, 0x10+(0x08*modelnumber));
		oldendpoint = oldheaderlength + oldendextraoffset;
	}
	copyBytes(oldgma, newgma, oldstartpoint, 0x40);
	//texture read and write, as well as copy
	uint16_t texturearray[0xff]; //no goals will be this long but it should be a generous measurement
	memset(texturearray, 0xff, sizeof(texturearray)); //255 initiation
	uint16_t texturearraypointer = 0;
	uint16_t materialamount = fileShortPluck(oldgma, oldstartpoint+0x18);
	uint64_t oldmodelheaderlength = 0x40;
	//Loop for each material
	for (uint32_t materialnumber = 0; materialnumber < materialamount; materialnumber++){
		copyBytes(oldgma, newgma, oldstartpoint+0x40+0x20*materialnumber, 0x04);
		uint16_t materialvalue = fileShortPluck(oldgma, oldstartpoint+0x44+0x20*materialnumber);
		uint16_t materialvaluepointer = *find(begin(texturearray), end(texturearray), materialvalue);
		uint16_t texturearrayendpointer = *end(texturearray);
		if (materialvaluepointer == texturearrayendpointer){
			//Not in array - add to array
			texturearray[texturearraypointer] = materialvalue;
			saveShortToFileEnd(newgma, texturearraypointer); 
			texturearraypointer += 1;
		} else {
			//In array - write value to array
			uint16_t texturevalueindex = distance(texturearray, find(begin(texturearray), end(texturearray), materialvalue));
			saveShortToFileEnd(newgma, texturevalueindex);
		}
		copyBytes(oldgma, newgma, oldstartpoint+0x46+0x20*materialnumber, 0x1A);
		oldmodelheaderlength += 0x20;
	}
	uint64_t oldmodeldatastart = oldstartpoint + oldmodelheaderlength;
	uint64_t oldmodeldatalength = oldendpoint - oldmodeldatastart;
	//rest of data
	copyBytes(oldgma, newgma, oldmodeldatastart, oldmodeldatalength);
	newgma.close();
	/*
	TPL
	*/
	ofstream newtpl(filename + "_" + suffix + ".tpl", ios::binary | ios::app);
	//init
	uint32_t textureamount = texturearraypointer;
	saveIntToFileEnd(newtpl, textureamount);
	//header loop
	//also creating rolling offset
	uint32_t oldtexturestarts[textureamount];
	uint32_t oldtextureends[textureamount];
	uint32_t rollingoffset = 0;
	for(int texturenumber = 0; texturenumber < textureamount; texturenumber++){
		uint16_t oldtexturevalue = texturearray[texturenumber];
		uint32_t oldtextureheaderpos =  oldtexturevalue*0x10+0x04;
		oldtexturestarts[texturenumber] = fileIntPluck(oldtpl, oldtextureheaderpos+0x04);
		if (oldtexturevalue < fileIntPluck(oldtpl, 0x0) - 1){
			//if it's less than this then the texture ends at the next value
			oldtextureends[texturenumber] = fileIntPluck(oldtpl, oldtextureheaderpos+0x14);
		} else {
			//else it ends at the end of the file
			oldtextureends[texturenumber] = getFileLength(oldtpl);
		}
		//copy initial bytes 
		copyBytes(oldtpl, newtpl, oldtextureheaderpos, 0x4);
		if (texturenumber == 0){
			//the first offset will always be the length of the header
			rollingoffset = (textureamount*0x10 + 0x10);
		} else {
			//the offset is based on the length of the previous one
			rollingoffset += (oldtextureends[texturenumber-1] - oldtexturestarts[texturenumber-1]);
		}
		saveIntToFileEnd(newtpl, rollingoffset);
		copyBytes(oldtpl, newtpl,  oldtextureheaderpos+0x08, 0x08);
	}
	//padding
	int tplpaddingamount = (- 0x04 + (0x10*textureamount)) % 0x20;
	for (uint8_t tplpaddingpointer = 0x0; tplpaddingpointer < tplpaddingamount; tplpaddingpointer++){
		newtpl << tplpaddingpointer;
	}
	//texture loop
	for(int texturenumber = 0; texturenumber < textureamount; texturenumber++){
		copyBytes(oldtpl, newtpl,  oldtexturestarts[texturenumber], oldtextureends[texturenumber]-oldtexturestarts[texturenumber]);
	}
	newtpl.close();
	std::cout << "saved to " << filename << "_" << suffix << endl;
}

int modelExtract(string filename, int type, string specificmodel){
	ifstream gma;
	ifstream tpl;
	//open files and check that they're good
	//
	gma.open(filename + ".gma", ios::binary);
	if (gma.good() == false) {
		cout << "No GMA found!" << endl;
		return -1;
	}
	tpl.open(filename + ".tpl", ios::binary);
	if (tpl.good() == false) {
		cout << "No TPL found!" << endl;
		return -1;
	}
	//If the files are good we can read the gma for the files
	uint32_t modelamount = fileIntPluck(gma, 0);
	uint32_t modellistoffset = modelamount * 0x8 + 0x8; //Start of model list - 0x8 initial bytes plus 0x8 for each model
	uint32_t modellistpointer = modellistoffset;
	uint32_t modelnamelength = 0;
	string modelname = "";
	if (type == 1){
		//Goal extraction block
		bool hasBlueGoal = false;
		bool hasGreenGoal = false;
		bool hasRedGoal = false;
		for (int modelnumber = 0; modelnumber < modelamount; modelnumber++){
			modelnamelength = getModelNameLength(gma, modellistpointer, 1);
			char bytes[modelnamelength];
			gma.seekg(modellistpointer, gma.beg);
			gma.read(bytes, modelnamelength);
			string modelname(bytes);
			if (hasBlueGoal == false){
				if (modelname.substr(3,5) == "_GOAL"){
					std::cout << modelname << " (Blue goal) ";
					modelWriteToFiles(filename, gma, tpl, modelamount, modelnumber, modelnamelength, modelname, "GOAL_B");
					hasBlueGoal = true;
				}
			} else if (hasGreenGoal == false){
				if (modelname.substr(3,7) == "_GOAL_G"){
					std::cout << modelname << " (Green goal) ";
					modelWriteToFiles(filename, gma, tpl, modelamount, modelnumber, modelnamelength, modelname, "GOAL_G");
					hasGreenGoal = true;
				}
			} else if (hasRedGoal == false){
				if (modelname.substr(3,7) == "_GOAL_R"){
					std::cout << modelname << " (Red goal) ";
					modelWriteToFiles(filename, gma, tpl, modelamount, modelnumber, modelnamelength, modelname, "GOAL_R");
					hasRedGoal = true;
				}
			}
			modellistpointer += (modelnamelength);
		}
		if (hasBlueGoal == false){
			cout << "No blue goal found!";
		}
	} else if (type == 2){
		//Switch extraction block
		bool hasSwitches = false;
		for (int modelnumber = 0; modelnumber < modelamount; modelnumber++){
			modelnamelength = getModelNameLength(gma, modellistpointer, 1);
			char bytes[modelnamelength];
			gma.seekg(modellistpointer, gma.beg);
			gma.read(bytes, modelnamelength);
			string modelname(bytes);
			if (modelname.substr(0,7) == "BUTTON_"){
				std::cout << modelname << " ";
				modelWriteToFiles(filename, gma, tpl, modelamount, modelnumber, modelnamelength, modelname, modelname);
				hasSwitches = true;
			}
			modellistpointer += (modelnamelength);
		}
		if (hasSwitches == false){
			cout << "No switches found!";
		}
	} else if (type == 3){
		//Specific model extraction block
		bool hasSpecificModel = false;
		for (int modelnumber = 0; modelnumber < modelamount; modelnumber++){
			modelnamelength = getModelNameLength(gma, modellistpointer, 1);
			char bytes[modelnamelength];
			gma.seekg(modellistpointer, gma.beg);
			gma.read(bytes, modelnamelength);
			string modelname(bytes);
			if (modelname == specificmodel){
				std::cout << modelname << " ";
				modelWriteToFiles(filename, gma, tpl, modelamount, modelnumber, modelnamelength, modelname, modelname);
				hasSpecificModel = true;
			}
			modellistpointer += (modelnamelength);
		}
		if (hasSpecificModel == false){
			cout << "The model " << specificmodel << " wasn't found!";
		}
	}
	gma.close();
	tpl.close();
	return 0;
}


/*

Part 2:
Model Merge

*/

int gmatplMerge(string filename1, string filename2){
	ifstream gma1;
	ifstream gma2;
	ifstream tpl1;
	ifstream tpl2;
	gma1.open(filename1 + ".gma", ios::binary);
	if (gma1.good() == false) {
		cout << "First GMA not found! (" << filename1 << ".gma)" << endl;
		return -1;
	}
	gma2.open(filename2 + ".gma", ios::binary);
	if (gma2.good() == false) {
		cout << "Second GMA not found! (" << filename2 << ".gma)" << endl;
		return -1;
	}
	tpl1.open(filename1 + ".tpl", ios::binary);
	if (tpl1.good() == false) {
		cout << "First TPL not found! (" << filename1 << ".tpl)" << endl;
		return -1;
	}
	tpl2.open(filename2 + ".tpl", ios::binary);
	if (tpl2.good() == false) {
		cout << "Second TPL not found! (" << filename2 << ".tpl)" << endl;
		return -1;
	}
	cout << "Merging GMAs and TPLs " << filename1 << " and " << filename2 << "..." << endl;
	//Remove old files
	remove((filename1 + "+" + filename2 + ".tpl").c_str());
	remove((filename1 + "+" + filename2 + ".gma").c_str());
	//First the GMA.
	//append number of models
	ofstream newgma(filename1 + "+" + filename2 + ".gma", ios::binary | ios::app);
	uint32_t gma1modelamount = fileIntPluck(gma1, 0x0);
	uint32_t gma2modelamount = fileIntPluck(gma2, 0x0);
	uint32_t newgmamodelamount = gma1modelamount + gma2modelamount;
	saveIntToFileEnd(newgma, newgmamodelamount);
	//Calculate length and start positions of gma1 and gma2 modellists
	uint32_t gma1nameliststart = 0x08*gma1modelamount + 0x08;
	uint32_t gma1lastnamestart = fileIntPluck(gma1, 0x08*gma1modelamount + 0x04) + gma1nameliststart;
	uint32_t gma1namelistend = getModelNameLength(gma1, gma1lastnamestart, 1) + gma1lastnamestart;
	uint32_t gma1namelistlength = gma1namelistend - gma1nameliststart;
	uint32_t gma2nameliststart = 0x08*gma2modelamount + 0x08;
	uint32_t gma2lastnamestart = fileIntPluck(gma2, 0x08*gma2modelamount + 0x04) + gma2nameliststart;
	uint32_t gma2namelistend = getModelNameLength(gma2, gma2lastnamestart, 1) + gma2lastnamestart + 1;
	uint32_t gma2namelistlength = gma2namelistend - gma2nameliststart;
	//With these we can create the new length of the GMA header
	//The pure header length is the initial bytes, plus the 8 times the number of of models, plus the sum of the lengths of the model name lists
	uint32_t newgmapureheaderlength = 0x8 + (newgmamodelamount)*0x8 + gma1namelistlength + gma2namelistlength;
	uint32_t newgmaheaderpadding = (-newgmapureheaderlength) % 0x20; //to pad it to 20
	uint32_t newgmaheaderlength = newgmapureheaderlength + newgmaheaderpadding;
	saveIntToFileEnd(newgma, newgmaheaderlength);
	//Here let's get the header and total lengths
	uint32_t gma1filelength = getFileLength(gma1);
	uint32_t gma1headerlength = fileIntPluck(gma1, 0x04);
	uint32_t gma1datalength = gma1filelength - gma1headerlength;
	uint32_t gma2filelength = getFileLength(gma2);
	uint32_t gma2headerlength = fileIntPluck(gma2, 0x04);
	uint32_t gma2datalength = gma2filelength - gma2headerlength;
	//The GMA1 bytes need no shifts as it comes first
	copyBytes(gma1, newgma, 0x8, gma1nameliststart-0x8);
	//The GMA2 bytes need an increase in both name list offset and data offset
	for (uint32_t gma2modelnumber = 0; gma2modelnumber < gma2modelamount; gma2modelnumber++){
		uint32_t gma2modeldataoffset = fileIntPluck(gma2, 0x8+0x8*gma2modelnumber);
		saveIntToFileEnd(newgma, gma2modeldataoffset + gma1datalength);
		uint32_t gma2modelnameoffset = fileIntPluck(gma2, 0xC+0x8*gma2modelnumber);
		saveIntToFileEnd(newgma, gma2modelnameoffset + gma1namelistlength);
	}
	//Model name lists
	copyBytes(gma1, newgma, gma1nameliststart, gma1namelistlength);
	copyBytes(gma2, newgma, gma2nameliststart, gma2namelistlength);
	//Padding
	padZeroes(newgma, newgmaheaderpadding);
	//GMA1 Model data. Can all be copied over.
	copyBytes(gma1, newgma, gma1headerlength, gma1datalength);
	//GMA2 Model data needs all of its textures shifted up
	//get number of textures from TPL1 and TPL2
	uint32_t tpl1textureamount = fileIntPluck(tpl1, 0x0);
	uint32_t tpl2textureamount = fileIntPluck(tpl2, 0x0);
	//loop for each header
	for (uint32_t gma2modelnumber = 0; gma2modelnumber < gma2modelamount; gma2modelnumber++){
		//i'm so lazy lol
		uint64_t oldstartextraoffset = (fileIntPluck(gma2, 0x08+(0x08*gma2modelnumber)));
		uint64_t oldstartpoint = gma2headerlength + oldstartextraoffset; //start of the model data
		uint64_t oldendpoint = 0;
		if (gma2modelamount == gma2modelnumber+1){ 
			oldendpoint = getFileLength(gma2);
		} else {
			uint64_t oldendextraoffset = fileIntPluck(gma2, 0x10+(0x08*gma2modelnumber));
			oldendpoint = gma2headerlength + oldendextraoffset;
		}
		uint16_t materialamount = fileShortPluck(gma2, oldstartpoint+0x18);
		copyBytes(gma2, newgma, oldstartpoint, 0x40);
		uint64_t oldmodelheaderlength = 0x40;
		//Loop for each material
		for (uint32_t materialnumber = 0; materialnumber < materialamount; materialnumber++) {
			copyBytes(gma2, newgma, oldstartpoint+0x40+0x20*materialnumber, 0x04);
			uint16_t materialvalue = fileShortPluck(gma2, oldstartpoint+0x44+0x20*materialnumber);
			saveShortToFileEnd(newgma, materialvalue + tpl1textureamount);
			copyBytes(gma2, newgma, oldstartpoint+0x46+0x20*materialnumber, 0x1A);
			oldmodelheaderlength += 0x20;
		}
		uint64_t oldmodeldatastart = oldstartpoint + oldmodelheaderlength;
		uint64_t oldmodeldatalength = oldendpoint - oldmodeldatastart;
		//rest of data
		copyBytes(gma2, newgma, oldmodeldatastart, oldmodeldatalength);
	}
	//we're done here
	newgma.close();
	//Now for the TPL
	ofstream newtpl(filename1 + "+" + filename2 + ".tpl", ios::binary | ios::app);
	//we can write the first byte straight away
	uint32_t newtpltextureamount = tpl1textureamount+tpl2textureamount;
	saveIntToFileEnd(newtpl, newtpltextureamount);
	//Now to work out the file header length 
	int newtplpaddingamount = (- 0x04 + (0x10*newtpltextureamount)) % 0x20;
	uint32_t newtplheaderlength = 0x04 + (0x10*newtpltextureamount) + newtplpaddingamount;
	uint32_t tpl1headerlength = fileIntPluck(tpl1, 0x08);
	uint32_t tpl2headerlength = fileIntPluck(tpl2, 0x08);
	uint32_t tpl1length = getFileLength(tpl1);
	uint32_t tpl2length = getFileLength(tpl2);
	for (uint32_t newtpltexturenumber = 0; newtpltexturenumber < newtpltextureamount; newtpltexturenumber++){
		if (newtpltexturenumber < tpl1textureamount){
			//take from tpl1
			copyBytes(tpl1, newtpl, newtpltexturenumber*0x10+0x04, 0x04);
			uint32_t oldtpl1textureoffset = fileIntPluck(tpl1, (newtpltexturenumber*0x10) + 0x08);
			saveIntToFileEnd(newtpl, oldtpl1textureoffset - tpl1headerlength + newtplheaderlength);
			copyBytes(tpl1, newtpl, (newtpltexturenumber*0x10) + 0x0C, 0x08);
		} else { //newtpltexturenumber >= tpl1textureamount
			//take from tpl2
			copyBytes(tpl2, newtpl, (newtpltexturenumber-tpl1textureamount)*0x10+0x04, 0x04);
			uint32_t oldtpl2textureoffset = fileIntPluck(tpl2, ((newtpltexturenumber-tpl1textureamount)*0x10) + 0x08);
			saveIntToFileEnd(newtpl, oldtpl2textureoffset - tpl2headerlength + tpl1length - tpl1headerlength + newtplheaderlength); //AAAAAA
			copyBytes(tpl2, newtpl, ((newtpltexturenumber-tpl1textureamount)*0x10)+0x0C, 0x08);
		}
	}
	for (uint8_t tplpaddingpointer = 0x0; tplpaddingpointer < newtplpaddingamount; tplpaddingpointer++){
		newtpl << tplpaddingpointer;
	}
	//remaining data bytes, tpl1
	if (tpl1textureamount != 0){
		copyBytes(tpl1, newtpl, tpl1headerlength, tpl1length - tpl1headerlength);
	}
	//tpl2
	if (tpl2textureamount != 0){
		copyBytes(tpl2, newtpl, tpl2headerlength, tpl2length - tpl2headerlength);
	}
	newtpl.close();
	gma1.close();
	tpl1.close();
	gma2.close();
	tpl2.close();
	return 0;
}

/*

Main body

*/

int main(int argc, char* argv[]){
	int successval = 1;
	if (argc >= 3) {
		string operationtype(argv[1]);
		if (argc == 3) {
			if (operationtype == "-ge") {
				string filename(argv[2]);
				successval = modelExtract(filename, 1, "");
			} else if (operationtype == "-se"){
				string filename(argv[2]);
				successval = modelExtract(filename, 2, "");
			}
		} else if (argc == 4) {
			if (operationtype == "-me") {
				string filename(argv[2]);
				string specificmodelname(argv[3]);
				successval = modelExtract(filename, 3, specificmodelname);
			} else if (operationtype == "-m") {
				string filename1(argv[2]);
				string filename2(argv[3]);
				successval = gmatplMerge(filename1, filename2);
			} else {successval = helpText();}
		} else {successval = helpText();}
	} else {successval = helpText();}
	if (successval == 0){
		cout << endl << "Done!";
	}
	return successval;
}