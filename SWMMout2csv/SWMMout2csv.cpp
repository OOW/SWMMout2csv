// SWMMout2csv.cpp : Defines the entry point for the console application.
// SWMMout2csv version working 0.8.6

#include "stdafx.h"
#include <stdio.h>     //printf, scanf, NULL 
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <windows.h>
#include <stdlib.h>     // malloc, free, rand 
#include <algorithm>
#include <functional>
#include <direct.h>
#include <exception>
#include <cstddef>
#include <map>

using namespace std;

// Define constants
//Define input file names
const char *version = "0.8.6";

const char *CSV_PARAMETER_INPUT;
const char *LOG_PATH;
const char *FILE_PATH_SUBCATCHMENTS;
const char *FILE_PATH_NODES;
const char *FILE_PATH_LINKS;
const char *FILE_PATH_SYSTEM;
const char *FILE_PATH_INPUT;

//globe variables(defined as global to maintain the value from function to function)
static FILE *FOUT_FILE = NULL; // File pointer
static int SUBCATCHMENT_COUNT, NODE_COUNT, LINK_COUNT, POLLUTANT_COUNT;
static int NAMES_OFFSET, PROPERTIES_OFFSET, RESULTS_OFFSET, NTIMESTEPS;
static int BYTES_PER_PERIOD, SUBCATCHMENT_TIMESTEP_SIZE, NODE_TIMESTEP_SIZE, LINK_TIMESTEP_SIZE;

int RECORD_BYTES = 4;
int NRECORDS_DAYS_SINCE_EPOCH = 2;
int DATETIME_BYTES = 8;
int SUBCATCHMENT_PAR_COUNT; // precip,Snow,losses,Runoff,GE flow, GE Ele
int NODE_PAR_COUNT; // depth,HGL,stored_ponded_volume,lateral_inflow,total_inflow,flooding 
int LINK_PAR_COUNT; // flow,depth,velocity,volume,capacity
int SYSTEM_PAR_COUNT;

//vector used to store the summary information for log.txt
map<string, size_t> SUMMARY_INFO;

//Defined Functions
// Extract flow rate; float type -4 bytes
float readFloat(FILE *f) {
	float v;
	fread((void*)(&v), sizeof(v), 1, f);
	return v;
}
// Extract parameters with integer types; int type - 4 bytes
int readInt(FILE *f) {
	int i;
	fread((void*)(&i), sizeof(i), 1, f);
	return i;
}
// Extract Timestep (Date Time); double type - 8 bytes
string readDateTimeDouble(FILE *f) {

	double dtime;
	fread((void*)(&dtime), sizeof(dtime), 1, f);

	// convert to SystemTime or DateTime
	// ref: (http://www.technical-recipes.com/2014/converting-a-systemtime-to-a-stdstring-in-c/)
	SYSTEMTIME systime;
	VariantTimeToSystemTime(dtime, &systime);
	char buffer[256];
	sprintf_s(buffer,
		"%04u-%02u-%02u %02d:%02d:%02d",
		systime.wYear,
		systime.wMonth,
		systime.wDay,
		systime.wHour,
		systime.wMinute,
		systime.wSecond);
	string dtime_formatted = buffer;

	return dtime_formatted;
}
// Extract link names
string readName(FILE *f, int name_bytes) {
	// assign character char the length of the expected element name
	char * buffer = new char[name_bytes + 1];
	buffer[name_bytes] = '\0'; // is required in order to end string
	fread(buffer, sizeof(char), (size_t)name_bytes, f);
	string mystring(buffer);
	free(buffer); // clean up 
	return mystring;
}
//Read Head
//Read id, version and flow unit, and write the summary information to log.txt
void readHead(FILE * f) {
	_fseeki64(f, 0, SEEK_SET);
	int idNumHead = readInt(f);
	int version = readInt(f);
	int flowunits = readInt(f); // not used currently
	SUMMARY_INFO.insert(make_pair("idNumHead", idNumHead));
	SUMMARY_INFO.insert(make_pair("version", version));
	SUMMARY_INFO.insert(make_pair("flowunits", flowunits));
	cout << "SWMM Version: " << version << endl << '\n';
	//cout << idNumHead << " " << version << " " << flowunits << endl;

	// Read number of elements
	SUBCATCHMENT_COUNT = readInt(f);
	NODE_COUNT = readInt(f);
	LINK_COUNT = readInt(f);
	POLLUTANT_COUNT = readInt(f);

	//_fseeki64(FOUT_FILE, 8, SEEK_CUR); // skip to link count, 8 bytes
	cout << "Elements Reported:" << endl;
	cout << "Subcatchments: " << SUBCATCHMENT_COUNT << endl;
	cout << "Nodes: " << NODE_COUNT << endl;
	cout << "Links: " << LINK_COUNT << endl;
	cout << "Pollutants: " << POLLUTANT_COUNT << endl << '\n';

}
//Read Tail
// Read section locations, timesteps, errorcode, and write them to log.txt
//move to the beginning of tail section
void readTail(FILE * f) {
	// Section locations, timesteps, errorcode
	//move to the beginning of tail section
	_fseeki64(FOUT_FILE, -24, SEEK_END); // RECORD_BYTES * NRECORDS_CLOSING = 4*6=24
	NAMES_OFFSET = readInt(f);
	PROPERTIES_OFFSET = readInt(f);
	RESULTS_OFFSET = readInt(f);
	NTIMESTEPS = readInt(f);
	int errorcode = readInt(f);
	int idNumTail = readInt(f);
	SUMMARY_INFO.insert(make_pair("errorcode", errorcode));
	SUMMARY_INFO.insert(make_pair("idNumTail", idNumTail));
}
// "int counts"  = nodesCounts... when skipping, make counts = 0 
void readVarCodeBytes(FILE *f, int counts) {
	int varCodeBytes = readInt(f);
	_fseeki64(f, varCodeBytes*RECORD_BYTES, SEEK_CUR);
	_fseeki64(f, counts*varCodeBytes*RECORD_BYTES, SEEK_CUR);
}
// Read the number of parameters of each element type
int readParCounts(FILE *f) {
	int parCounts = readInt(f);
	_fseeki64(f, parCounts*RECORD_BYTES, SEEK_CUR);
	return parCounts;
}

// Read properties section
void readProperties(FILE *f) {
	_fseeki64(FOUT_FILE, PROPERTIES_OFFSET, SEEK_SET);
	// find the number of system elements; changes for different SWMM versions	
	// skip first section; Need to investigate what this section contains, 
	//see SWMM outputOpen in output.c code
	// Subcatchments,nodes,links
	readVarCodeBytes(FOUT_FILE, SUBCATCHMENT_COUNT);
	readVarCodeBytes(FOUT_FILE, NODE_COUNT);
	readVarCodeBytes(FOUT_FILE, LINK_COUNT);
	// read parameter counts
	SUBCATCHMENT_PAR_COUNT = readParCounts(FOUT_FILE);
	NODE_PAR_COUNT = readParCounts(FOUT_FILE);
	LINK_PAR_COUNT = readParCounts(FOUT_FILE);
	// read number of system parameters
	SYSTEM_PAR_COUNT = readInt(FOUT_FILE);
	int system_byte_count = SYSTEM_PAR_COUNT * RECORD_BYTES;

	SUBCATCHMENT_TIMESTEP_SIZE = SUBCATCHMENT_COUNT * SUBCATCHMENT_PAR_COUNT * RECORD_BYTES;
	NODE_TIMESTEP_SIZE = NODE_COUNT * NODE_PAR_COUNT * RECORD_BYTES;
	LINK_TIMESTEP_SIZE = LINK_COUNT * LINK_PAR_COUNT * RECORD_BYTES;
	BYTES_PER_PERIOD =
		DATETIME_BYTES +
		SUBCATCHMENT_TIMESTEP_SIZE +
		NODE_TIMESTEP_SIZE +
		LINK_TIMESTEP_SIZE +
		system_byte_count;
}
// check if file exists
void check_file_exist(string filename) {
	ifstream file;
	file.exceptions(ifstream::failbit | ifstream::badbit);
	try {
		file.open(filename);
	}
	catch (ifstream::failure e) {
		cout << "ERROR: " << filename << " doesn't exist." << endl;
		exit(0);
	}
	file.close();
}

// get current working directory
string workingdir() {
	char buf[256];
	GetCurrentDirectoryA(256, buf);
	return string(buf) + '\\';
}

//Read "model_post_process_input_parameters.csv"
map<string, string> readCSVContent(const char * CSV_PARAMETER_INPUT) {
	string parameterName, filePath, notes;
	map<string, string> parameterList;
	ifstream file; file.open(CSV_PARAMETER_INPUT);

	while (file.good())
	{
		//Escape the parameter name column
		getline(file, parameterName, ',');
		// Read the file path and other parameters
		getline(file, filePath, ',');
		//escape the notes column
		getline(file, notes, '\n');
		// remove trailing spaces
		// convert all whitespace characters to a standard space
		std::replace_if(filePath.begin(), filePath.end(), (std::function<int(BYTE)>)::isspace, ' ');
		if (filePath != "") {
			size_t f = filePath.find_first_not_of(' ');
			filePath = filePath.substr(f, filePath.find_last_not_of(' ') - f + 1);
		}
		parameterList.insert(make_pair(parameterName, filePath));
	}

	file.close();

	// check extraction options(extract Subcatchments, extract Nodes, extract Links, extract System)
	// 1= TRUE and 0 = FALSE
	vector<string> names{ "extract Subcatchments","extract Nodes","extract Links", "extract System" };
	for (size_t i = 0; i < names.size(); i++) {
		try
		{
			if (parameterList[names[i]] == "" || (parameterList[names[i]] != "0" && parameterList[names[i]] != "1"))
			{
				throw "ERROR: Invalid option for \"";
			}
		}
		catch (const char * e)
		{
			cerr << e << names[i] << "\"!" << endl;
			exit(0);
		}
	}
	return parameterList;
}
//read in multiple SWMM output files from input parameter .csv("model_post_process_input_parameters.csv")
vector<string> readMultipleSWMMOutput(string fileNames) {
	stringstream ss_input(fileNames);
	string temp;
	vector<string> SWMMOutputs;

	// check if at least one SWMM output file has been supplied in input parameter .csv
	// if no, throw exception
	try
	{
		if (fileNames != "") {

			while (getline(ss_input, temp, ';'))
			{
				std::replace_if(temp.begin(), temp.end(), (std::function<int(BYTE)>)::isspace, ' ');
				size_t f = temp.find_first_not_of(' ');
				temp = temp.substr(f, temp.find_last_not_of(' ') - f + 1);
				SWMMOutputs.push_back(temp);
			}
		}
		else {
			throw "SWMM .out input file is requried!";
		}
	}
	catch (const char *e)
	{
		cerr << e << endl;
	}
	return SWMMOutputs;
}
// Open SWMM output file in binary mode
void outputOpen(const char *filePath) {
	try
	{   //check if SWMM .out file exists
		check_file_exist(filePath);
		// Open the file in binary mode
		int eCount = fopen_s(&FOUT_FILE, filePath, "rb");

		// Check if input file was opened correctly
		if (eCount != 0)
			throw "Cannot open SWMM output ";
		else
			cout << filePath << " opened successfully" << endl << '\n';
	}
	catch (const char* e)
	{
		cerr << "ERROR: " << e << filePath << endl;
		exit(0);
	}
}

// Read selected elements from input files e.g "nodes.txt"
vector<string> readSelectedElements(const char *filePath) {
	vector<string> nameList;
	try
	{
		if (filePath != NULL && filePath[0] != '\0')
		{
			string line; // temporary storage of line value
						 // Read node lists from input text file
						 //if no element files (subcatchmentsFileName,nodesFileName,linksFileName) were supplied in input parameter .csv
						 // leave vector "namelist" blank to read all reported elements
			ifstream par; par.open(filePath);
			if (!par)
			{
				throw " Cannot open ";
			}
			while (getline(par, line)) {
				// remove trailing spaces
				// convert all whitespace characters to a standard space
				std::replace_if(line.begin(), line.end(), (std::function<int(BYTE)>)::isspace, ' ');
				//remove the sapce in line and modify the length of the string(http://stackoverflow.com/questions/83439/remove-spaces-from-stdstring-in-c)
				line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
				if (line != "") {
					size_t f = line.find_first_not_of(' ');
					line = line.substr(f, line.find_last_not_of(' ') - f + 1);
					// add new name to vector
					nameList.push_back(line);
				}
			}
		}
	}
	catch (const char * e)
	{
		cout << "Cannot open " << filePath << endl;
		exit(0);
	}
	return nameList;
}
// Format date time 
string dateTimeListFormat(string dateTimeString) {

	SYSTEMTIME systime;
	memset(&systime, 0, sizeof(systime));
	// Date string should be "mm/dd/yyyy hh:mm";
	const char *foo = dateTimeString.c_str();

	int MM = 999, DD = 999, YYYY = 999, hh = 999, mm = 999;
	sscanf_s(foo, "%d/%d/%d %d:%d",
		&MM,
		&DD,
		&YYYY,
		&hh,
		&mm);
	int ss = 0; //hardcode seconds to 0

	try {
		if (MM != 999 &&
			DD != 999 &&
			YYYY != 999 &&
			hh != 999 &&
			mm != 999) {
			//User supplied dateTime is formatted correctly
		}
		else {
			throw
				"Start/End DateTime formatted incorrectly; requires MM/DD/YYYY hh:mm";
		}
	}
	catch (const char* e)
	{
		cerr << "ERROR: " << e << endl;
		exit(0);
	}

	//format datetime string to SWMM formatting
	char buffer[256];
	sprintf_s(buffer,
		"%04u-%02u-%02u %02d:%02d:%02d",
		YYYY,
		MM,
		DD,
		hh,
		mm,
		ss);

	string dateTimeStringFinal = buffer;

	return dateTimeStringFinal;

}
// Get variable extraction options for each element
vector<size_t> readSelectedVariables(string parameterList) {
	vector<size_t> variables;
	stringstream ss(parameterList);
	size_t i;
	while (ss >> i)
	{
		if (ss.peek() == ';') {
			ss.ignore();
		}
		try
		{
			if (i == 0 || i == 1)
			{
				variables.push_back(i);
			}
			else
			{
				throw "ERROR : Invalid input in variables options ";
			}
		}
		catch (const char * e)
		{
			cerr << e << "NOTICE: Please enter 0 or 1!" << endl;
			exit(0);
		}
	}
	return variables;
}
// Check if the entries for variable selection match the number of variables
// the last entry is the option for pollutants, if 0, do not export any pollutant results, if 1, export all pollutants
vector<size_t> checkVariables(vector<size_t>& variables, int parCount, const char * filePath) {
	string  variableName;
	// set variable name for exception
	if (filePath == FILE_PATH_SUBCATCHMENTS) { variableName = "subcatchments"; }
	if (filePath == FILE_PATH_NODES) { variableName = "nodes"; }
	if (filePath == FILE_PATH_LINKS) { variableName = "links"; }

	if (variables.size() != (parCount - POLLUTANT_COUNT + 1)) {
		cout << "ERROR: the number of your entries doesn't match the number of " << variableName << " variables" << endl;
		exit(0);
	}
	// if need to export pollutants (last entry is 1),
	// if water quality is turned off, POLLUTANT_COUNT wuold be equal to 0
	// if POLLUTANT_ CONUT == 0, remove the option for exporting pollutant data from variable vector no matter it's 0 or 1.

	if (POLLUTANT_COUNT == 0)
	{
		variables.pop_back();

	}
	else if (variables.back() == 1)
	{
		// if water quality is turned on and user want to export pollutant data, add (POLLUTANT_COUNT-1) options to the end of variables vector, make each pollutant has a respective option 
		for (size_t i = 0; i < POLLUTANT_COUNT - 1; i++)
		{
			variables.push_back(1);
		}
	}
	return variables;
}
// Create output file names
string generateFileName(const char * filePath, size_t i, vector<string>& reportPollutantNames) {
	string filename;
	string variables;
	string outFileName;
	// Extract file name (without extension) from user-supplied file path
	//transfer FILE_PATH_INPUT from char to string in order to use rfind() 
	stringstream tempStream(FILE_PATH_INPUT);
	string temp = tempStream.str();
	// use rfind() to find the position of the last occurrence of "\\"
	size_t position = temp.rfind("\\");
	// extract file name from full path
	string middle = temp.substr(position + 1);
	//stringstream middle(temp.substr(position + 1));
	// get the file name(without extension)
	//getline(middle, outFileName, '.');

	// find the position of the last occurence of "."
	size_t position_1 = middle.rfind("\.");
	outFileName = middle.substr(0, position_1);
	stringstream ss;

	// add extension to file name
	// for log.txt
	if (filePath == LOG_PATH)
	{
		variables = "_log.txt";
		ss << outFileName << variables;
		filename = ss.str();
	}
	else
	{
		if (filePath == FILE_PATH_SUBCATCHMENTS) {
			// in SWMM 5.1, subcatchment has 9 variables including pollutants
			// in SWMM 5.0, subcatchment has 7 variables including pollutants
			if (SUMMARY_INFO["version"] >= 51000)
			{
				switch (i)
				{
				case 0: variables = "_subcatchment_precipitation"; break;
				case 1: variables = "_subcatchment_snow_Depth"; break;
				case 2: variables = "_subcatchment_evaporation"; break;
				case 3: variables = "_subcatchment_infiltration"; break;
				case 4: variables = "_subcatchment_runoff"; break;
				case 5: variables = "_subcatchment_gw_flow"; break;
				case 6: variables = "_subcatchment_gw_ele"; break;
				case 7: variables = "_subcatchment_soil_moisture"; break;
				default:
					break;
				}
				if (i > 7)
				{
					variables = "_subcatchment_" + reportPollutantNames[i - 8];
				}
			}
			else
			{
				switch (i)
				{
				case 0: variables = "_subcatchment_precipitation"; break;
				case 1: variables = "_subcatchment_snow_Depth"; break;
				case 2: variables = "_subcatchment_evap_infil"; break;
				case 3: variables = "_subcatchment_runoff"; break;
				case 4: variables = "_subcatchment_gw_flow"; break;
				case 5: variables = "_subcatchment_gw_ele"; break;
				default:
					break;
				}
				if (i > 5)
				{
					variables = "_subcatchment_" + reportPollutantNames[i - 6];
				}
			}
		}
		if (filePath == FILE_PATH_NODES) {
			switch (i)
			{
			case 0: variables = "_node_water_depth"; break;
			case 1: variables = "_node_HGL"; break;
			case 2: variables = "_node_stored_water_volume"; break;
			case 3: variables = "_node_later_inflow"; break;
			case 4: variables = "_node_total_flow"; break;
			case 5: variables = "_node_surface_flooding"; break;
			default:
				break;
			}
			if (i > 5)
			{
				variables = "_node_" + reportPollutantNames[i - 6];
			}
		}
		if (filePath == FILE_PATH_LINKS) {
			switch (i)
			{
			case 0: variables = "_link_flow_rate"; break;
			case 1: variables = "_link_depth"; break;
			case 2: variables = "_link_velocity"; break;
			case 3: variables = "_link_volume"; break;
			case 4: variables = "_link_capacity"; break;
			default:
				break;
			}
			if (i > 4)
			{
				variables = "_link_" + reportPollutantNames[i - 5];
			}
		}
		if (filePath == FILE_PATH_SYSTEM)
		{
			variables = "_system_variables";
		}
		// for other files
		ss << outFileName << variables << ".csv";
		filename = ss.str();
	}

	return filename;
}
// Subset selected variables
vector<size_t> subsetSelectdElements(vector<string>& variableNameList, vector<string>& reportVariableNames, const char * filePath) {
	vector<size_t> selectedVariableIndex;
	if (variableNameList.size() == 0) { // read all reported nodes if input file was empty
		for (size_t n = 0; n < reportVariableNames.size(); ++n) {
			selectedVariableIndex.push_back(n);
		}
		variableNameList = reportVariableNames;
	}
	else { // search for matching Nodes; case sensitive
		   //for (size_t n = 0; n < reportNodeNames.size(); ++n) { // n is report Node
		for (size_t i = 0; i < variableNameList.size(); ++i) { // i is input selected Node

			auto position = find(reportVariableNames.begin(), reportVariableNames.end(), variableNameList[i]);
			try
			{
				if (position == reportVariableNames.end())
				{
					throw
						" can't be found in SWMM's report section.Please check the input in ";
				}
				size_t index = position - reportVariableNames.begin();
				selectedVariableIndex.push_back(index);
			}
			catch (const char* e)
			{
				cerr << endl << "ERROR: " << variableNameList[i] << e << filePath << endl << endl;
				exit(0);
			}
		}
	}
	return selectedVariableIndex;
}
// Extract selected time period
vector<size_t> subsetSelectdTime(vector<string>& dateTimeList) {
	vector<size_t> selectedTimeIndex;
	bool startTimeFound = FALSE;
	bool endTimeFound = FALSE;

	// If no user-supplid start time , use the reported start time in bianry file
	if (dateTimeList[0] == "")
	{
		selectedTimeIndex.push_back(0);
		startTimeFound = TRUE;
	}
	// loop to match the start/end time with reported datetime
	for (size_t n = 0; n < NTIMESTEPS; ++n) {
		_int64 bytePos = RESULTS_OFFSET + (n * BYTES_PER_PERIOD);
		_fseeki64(FOUT_FILE, bytePos, SEEK_SET);
		string timeCompare = readDateTimeDouble(FOUT_FILE);

		//Add index to vector selectedTimeIndex when start/end time are both supplied
		if (dateTimeList[0] != "" && dateTimeList[1] != "")
		{
			// if start datetime is found, continue "for" loop to look for the end datetime
			if (dateTimeList[0] == timeCompare)
			{
				selectedTimeIndex.push_back(n);
				startTimeFound = TRUE;
				continue;
			}
			// if end datetime is found, end "for" loop
			if (dateTimeList[1] == timeCompare)
			{
				selectedTimeIndex.push_back(n);
				endTimeFound = TRUE;
				break;
			}
		}
		if (dateTimeList[0] != "" && dateTimeList[1] == "")
		{
			if (dateTimeList[0] == timeCompare)
			{
				selectedTimeIndex.push_back(n);
				startTimeFound = TRUE;
				break;
			}
		}
		if (dateTimeList[0] == "" && dateTimeList[1] != "")
		{
			if (dateTimeList[1] == timeCompare)
			{
				selectedTimeIndex.push_back(n);
				endTimeFound = TRUE;
				break;
			}
		}
	}
	// if user didn't supply the end time, then use the reported end time in binary file
	if (dateTimeList[1] == "")
	{
		selectedTimeIndex.push_back(NTIMESTEPS - 1);
		endTimeFound = TRUE;
	}
	// check exceptions for "Not found"
	try
	{
		if (startTimeFound == FALSE || endTimeFound == FALSE) {
			throw
				" can't be found in SWMM's report section.Please check the Start/End DateTime in ";
		}
	}
	catch (const char* e)
	{
		if (startTimeFound == FALSE && endTimeFound == FALSE)
		{
			cerr << "ERROR: " << dateTimeList[0] << " and " << dateTimeList[1] << e << CSV_PARAMETER_INPUT << endl;
			exit(0);
		}
		else if (startTimeFound == FALSE)
		{
			cerr << "ERROR: " << dateTimeList[0] << e << CSV_PARAMETER_INPUT << endl;
			exit(0);
		}
		else if (endTimeFound == FALSE)
		{
			cerr << "ERROR: " << dateTimeList[1] << e << CSV_PARAMETER_INPUT << endl;
			exit(0);
		}
	}

	return selectedTimeIndex;
}
//Read and write extraction results to output files
void writeOutput(vector<size_t>& elementsVariables, vector<size_t>& selectedElementsIndex, vector<string>& reportElementNames, vector<string>& reportPollutantNames, vector<size_t>& selectedTimeIndex, const char * filePath) {
	// for each variable
	for (size_t i = 0; i < elementsVariables.size(); ++i) {
		// if the variable has been selected
		if (elementsVariables[i] == 1) {
			//Get the output file name
			string filename = generateFileName(filePath, i, reportPollutantNames);
			string resultsPath = filename.c_str();
			ofstream resultsOutPath; resultsOutPath.open(resultsPath);

			// Write headers to output file
			resultsOutPath << "DateTime,";
			for (size_t n = 0; n < selectedElementsIndex.size(); ++n) {
				resultsOutPath << reportElementNames[selectedElementsIndex[n]];
				if (n < (selectedElementsIndex.size() - 1)) {
					resultsOutPath << ",";
				}
			}
			resultsOutPath << endl;
			// Write time steps to output file
			for (_int64 t = selectedTimeIndex.front(); t <= selectedTimeIndex.back(); ++t) {
				_int64 bytePos = RESULTS_OFFSET + (t * BYTES_PER_PERIOD);
				_fseeki64(FOUT_FILE, bytePos, SEEK_SET);
				string dtime = readDateTimeDouble(FOUT_FILE);

				resultsOutPath << dtime;
				for (size_t n = 0; n < selectedElementsIndex.size(); ++n) {
					// Add comma
					resultsOutPath << ",";
					// compute byte distance
					_int64 bytePos = RESULTS_OFFSET + (t * BYTES_PER_PERIOD);
					bytePos += DATETIME_BYTES;

					if (filePath == FILE_PATH_SUBCATCHMENTS)
					{
						bytePos += (selectedElementsIndex[n] * SUBCATCHMENT_PAR_COUNT * RECORD_BYTES);
					}
					if (filePath == FILE_PATH_NODES)
					{
						bytePos += SUBCATCHMENT_TIMESTEP_SIZE;
						bytePos += (selectedElementsIndex[n] * NODE_PAR_COUNT * RECORD_BYTES);
					}
					else if (filePath == FILE_PATH_LINKS)
					{
						bytePos += SUBCATCHMENT_TIMESTEP_SIZE;
						bytePos += NODE_TIMESTEP_SIZE;
						bytePos += (selectedElementsIndex[n] * LINK_PAR_COUNT * RECORD_BYTES);
					}

					size_t varIndex = i;
					bytePos += varIndex * RECORD_BYTES;
					_fseeki64(FOUT_FILE, bytePos, SEEK_SET);
					//read value
					resultsOutPath << readFloat(FOUT_FILE);
				}
				// end timestep line
				resultsOutPath << endl;
				resultsOutPath.flush(); // should reduce RAM
			}
			resultsOutPath.close();
		}
	}
}

// combine user-supplied input/output filepath with filename
string combinePath(string filepath, string filename) {
	if (filepath.empty() || filename.empty())
	{
		return filename;
	}
	else
	{
		check_file_exist(filepath + "\\" + filename);
		return (filepath + "\\" + filename);
	}
}
// write summary information log.txt
void writeLogTXT(string LOG_PATH, double elapsed_mins) {

	ofstream logOut; logOut.open(LOG_PATH);
	//vector SUMMARY_INFO: 0 inNumHead, 1 version, 2 flowunits, 3 errorcode, 4 idNumTail, 5 extractSubcatchments, 6 extractNodes, 7 extractLinks, 8 extractSystem, 

	// Write the information read from head section to log.txt
	logOut << "SWMM Version: " << SUMMARY_INFO["version"] << endl << '\n';
	//logOut << idNumHead << " " << version << " " << flowunits << endl;
	logOut << "Elements Reported:" << endl;
	//logOut << SUMMARY_INFO[0] << " " << SUMMARY_INFO[1] << " " << SUMMARY_INFO[2] << endl;
	logOut << "Subcatchments: " << SUBCATCHMENT_COUNT << endl;
	logOut << "Nodes: " << NODE_COUNT << endl;
	logOut << "Links: " << LINK_COUNT << endl;
	logOut << "Pollutants: " << POLLUTANT_COUNT << endl << '\n';

	logOut << "Elements Extracted:" << endl;
	logOut << "Subcatchments: " << SUMMARY_INFO["subcatchmentsExtracted"] << endl;
	logOut << "Nodes: " << SUMMARY_INFO["nodesExtracted"] << endl;
	logOut << "Links: " << SUMMARY_INFO["linksExtracted"] << endl;
	logOut << "System: " << SUMMARY_INFO["systemExtracted"] << endl << '\n';


	//Write the information read from tail section to log.txt
	logOut << "Timesteps: " << NTIMESTEPS << "; Error Code: " << SUMMARY_INFO["errorcode"] << "; ID NUM: " << SUMMARY_INFO["idNumTail"] << endl;
	logOut << "Complete, Elapsed Time (min): "; logOut << elapsed_mins << endl;
	logOut.close();
}



// main function
int main(int argc, char* argv[])
{

	const char *outputPath;
	const char *inputPath;

	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	cout << "                            Binary File Reader                                " << endl;
	cout << "                             version: " << version << "                        " << endl;
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

	//Read parameter from batch file
	//or use default input file "SWMMout2csv_input_086.csv"
	if (argc == 1) {
		CSV_PARAMETER_INPUT = "SWMMout2csv_input_086.csv";
	}
	if (argc == 2) {
		CSV_PARAMETER_INPUT = argv[1];
	}

	// Check if "SWMMout2csv_input_086.csv" exists in current working directory
	check_file_exist(CSV_PARAMETER_INPUT);

	// Read input parameters from  "SWMMout2csv_input_086.csv" 
	map<string, string> parameterList = readCSVContent(CSV_PARAMETER_INPUT);

	outputPath = parameterList["reader_output_path"].c_str();
	inputPath = parameterList["reader_input_path"].c_str();

	// "SWMMoutFileName" has multiple inputs(separated by ";"), save them into vector "SWMMOutputs"
	vector<string> SWMMOutputs = readMultipleSWMMOutput(parameterList["SWMMoutFileName"]);


	//get working directory
	string wd = workingdir();

	// for each input in "SWMMoutFileName"
	for (size_t i = 0; i < SWMMOutputs.size(); i++)
	{
		// set wd back to the original working directory
		_chdir(wd.c_str());
		clock_t begin = clock(); // save start time
								 //Read in input files and start/end time
		string combinedPath = combinePath(inputPath, SWMMOutputs[i]);
		FILE_PATH_INPUT = combinedPath.c_str();
		string combinedPathSubcatchment = combinePath(inputPath, parameterList["subcatchmentsFileName"]);
		FILE_PATH_SUBCATCHMENTS = combinedPathSubcatchment.c_str();
		string combinedPathNode = combinePath(inputPath, parameterList["nodesFileName"]);
		FILE_PATH_NODES = combinedPathNode.c_str();
		string combinedPathLink = combinePath(inputPath, parameterList["linksFileName"]);
		FILE_PATH_LINKS = combinedPathLink.c_str();
		// initialize FILE_PATH_SYSTEM, just used for file type recognition in function "generateFileName"
		FILE_PATH_SYSTEM = "";
		string startTime = parameterList["reader_startDateTime"];
		string endTime = parameterList["reader_endDateTime"];

		// Get options for element extraction(0 = FALSE, 1 = TRUE)
		int	extractSubcatchments = stoi(parameterList["extract Subcatchments"]);
		int	extractNodes = stoi(parameterList["extract Nodes"]);
		int extractLinks = stoi(parameterList["extract Links"]);
		int	extractSystem = stoi(parameterList["extract System"]);

		//Get selected varibales for each element (0 = FALSE, 1 = TRUE)
		vector<size_t> subcatchmentVariables = readSelectedVariables(parameterList["Subcatchment variables"]);
		vector<size_t> nodeVariables = readSelectedVariables(parameterList["Node variables"]);
		vector<size_t> linkVariables = readSelectedVariables(parameterList["Link variables"]);
		//vector<size_t> sysVariables;

		// Read SWMM Binary .out file -- open the file in binary mode, check if input file was opened correctly
		outputOpen(FILE_PATH_INPUT);

		// Read selected elements from input files
		vector<string> subcatchmentsNameList = readSelectedElements(FILE_PATH_SUBCATCHMENTS);
		vector<string> nodesNameList = readSelectedElements(FILE_PATH_NODES);
		vector<string> linksNameList = readSelectedElements(FILE_PATH_LINKS);
		vector<string> dateTimeList;

		//Time period selection
		if (startTime == "" && endTime != "") {
			endTime = dateTimeListFormat(endTime);
		}
		if (startTime != "" && endTime == "")
		{
			startTime = dateTimeListFormat(startTime);
		}
		if (startTime != "" && endTime != "") {
			startTime = dateTimeListFormat(startTime);
			endTime = dateTimeListFormat(endTime);
			//test if start datetime is later than end datetime
			try
			{
				if (!(startTime < endTime)) {
					throw "ERROR: Invalid time period. Please check start/end time in ";
				}
			}
			catch (const char * e)
			{
				cerr << e << CSV_PARAMETER_INPUT << endl;
				exit(0);
			}
		}

		dateTimeList.push_back(startTime);
		dateTimeList.push_back(endTime);

		// Start reading SWWM Output file ####
		// Read Head
		readHead(FOUT_FILE);
		// Read Tail
		readTail(FOUT_FILE);
		// Read Properties Section
		readProperties(FOUT_FILE);

		// Build headers
		cout << "Reading Link Names..." << endl;

		// Read Reported Element Names
		// jump to the start position
		_fseeki64(FOUT_FILE, NAMES_OFFSET, SEEK_SET);

		// Read Subcatchment Names
		vector<string> reportSubcatchmentNames(SUBCATCHMENT_COUNT); // stores reported links
		for (int n = 0; n < SUBCATCHMENT_COUNT; ++n) {
			int name_bytes = readInt(FOUT_FILE);
			reportSubcatchmentNames[n] = readName(FOUT_FILE, name_bytes);
		}
		// Read Node Names
		vector<string> reportNodeNames(NODE_COUNT); // stores reported nodes
		for (int n = 0; n < NODE_COUNT; ++n) {
			int name_bytes = readInt(FOUT_FILE);
			reportNodeNames[n] = readName(FOUT_FILE, name_bytes);
		}
		// Read Link Names
		vector<string> reportLinkNames(LINK_COUNT); // stores reported links
		for (size_t n = 0; n < LINK_COUNT; ++n) {
			int name_bytes = readInt(FOUT_FILE);
			reportLinkNames[n] = readName(FOUT_FILE, name_bytes);
		}

		// Read Pollutant Names
		vector<string> reportPollutantNames(POLLUTANT_COUNT);
		for (size_t n = 0; n < POLLUTANT_COUNT; n++)
		{
			int name_bytes = readInt(FOUT_FILE);
			reportPollutantNames[n] = readName(FOUT_FILE, name_bytes);
		}


		cout << "Matching element names..." << endl;
		//get indexes of selected elements, these indexes will be used to extract selected elemets from reported elements later
		vector<size_t> selectedSubcatchmentsIndex = subsetSelectdElements(subcatchmentsNameList, reportSubcatchmentNames, FILE_PATH_SUBCATCHMENTS);
		vector<size_t> selectedNodesIndex = subsetSelectdElements(nodesNameList, reportNodeNames, FILE_PATH_NODES);
		vector<size_t> selectedLinksIndex = subsetSelectdElements(linksNameList, reportLinkNames, FILE_PATH_LINKS);
		cout << "Determining start/end date-time..." << endl;
		vector<size_t> selectedTimeIndex = subsetSelectdTime(dateTimeList);

		//Start to write the extracted information to ouput files
		// Extract Results
		cout << "Extracting Results..." << endl;

		// Change to user-supplied output path
		// Leave as current path if no path was supplied by user
		string outputPathAsString = outputPath;

		if (outputPathAsString != "") {
			_chdir(outputPath);
		}

		// variables to indicate how many elements have been extracted
		size_t subcatchmentsExtracted = 0;
		size_t nodesExtracted = 0;
		size_t linksExtracted = 0;
		size_t systemExtracted = 0;
		// write subcatchments 
		if (extractSubcatchments == 1 && selectedSubcatchmentsIndex.size() > 0)
		{
			//check if the number of entries and variables match
			subcatchmentVariables = checkVariables(subcatchmentVariables, SUBCATCHMENT_PAR_COUNT, FILE_PATH_SUBCATCHMENTS);
			// get the number of elements extracted, and add to vectot SUMMARY.INFO
			subcatchmentsExtracted = selectedSubcatchmentsIndex.size();
			SUMMARY_INFO.insert(make_pair("subcatchmentsExtracted", subcatchmentsExtracted));
			// Write the extraction reulsts to the output file
			writeOutput(subcatchmentVariables, selectedSubcatchmentsIndex, reportSubcatchmentNames, reportPollutantNames, selectedTimeIndex, FILE_PATH_SUBCATCHMENTS);

		}

		// write node 
		if (extractNodes == 1 && selectedNodesIndex.size() > 0)
		{
			nodeVariables = checkVariables(nodeVariables, NODE_PAR_COUNT, FILE_PATH_NODES);
			// get the number of elements extracted, and add to vectot SUMMARY.INFO
			nodesExtracted = selectedNodesIndex.size();
			SUMMARY_INFO.insert(make_pair("nodesExtracted", nodesExtracted));
			// Write the extraction reulsts to the output file
			writeOutput(nodeVariables, selectedNodesIndex, reportNodeNames, reportPollutantNames, selectedTimeIndex, FILE_PATH_NODES);
		}


		//  write links
		if (extractLinks == 1 && selectedLinksIndex.size() > 0)
		{
			linkVariables = checkVariables(linkVariables, LINK_PAR_COUNT, FILE_PATH_LINKS);
			linksExtracted = selectedLinksIndex.size();
			SUMMARY_INFO.insert(make_pair("linksExtracted", linksExtracted));
			// Write the extraction reulsts to the output file
			writeOutput(linkVariables, selectedLinksIndex, reportLinkNames, reportPollutantNames, selectedTimeIndex, FILE_PATH_LINKS);
		}

		//System
		if (extractSystem == 1) {

			systemExtracted = 1;
			SUMMARY_INFO.insert(make_pair("systemExtracted", systemExtracted));

			// create and open system variables output file, 0 is a place holder here
			string filename = generateFileName(FILE_PATH_SYSTEM, 0, reportPollutantNames);
			const char *sysResultsPath = filename.c_str();
			cout << FILE_PATH_SYSTEM << endl;
			ofstream sysResultsOut; sysResultsOut.open(sysResultsPath);

			//System Headers
			//Add DateTime header
			sysResultsOut << "DateTime,";
			if (SYSTEM_PAR_COUNT == 14) {
				sysResultsOut <<
					"air_temperature,"
					"rainfall_intensity,"
					"snow_depth,"
					"infiltration,"
					"runoff_flow,"
					"dry_weather_inflow,"
					"groundwater_inflow,"
					"RDII_inflow,"
					"external_inflow,"
					"total_lateral_inflow,"
					"flooding_outflow,"
					"outfall_outflow,"
					"storage_volume,"
					"evaporation,"
					//"daily potential evapotranspiration"
					<< endl;
			}
			if (SYSTEM_PAR_COUNT == 15) {
				sysResultsOut <<
					"air_temperature,"
					"rainfall_intensity,"
					"snow_depth,"
					"infiltration,"
					"runoff_flow,"
					"dry_weather_inflow,"
					"groundwater_inflow,"
					"RDII_inflow,"
					"external_inflow,"
					"total_lateral_inflow,"
					"flooding_outflow,"
					"outfall_outflow,"
					"storage_volume,"
					"evaporation,"
					"potential ET"
					<< endl;
			}

			for (_int64 t = selectedTimeIndex.front(); t <= selectedTimeIndex.back(); ++t) {
				// read timestep	
				_int64 bytePos = RESULTS_OFFSET + (t * BYTES_PER_PERIOD);
				_fseeki64(FOUT_FILE, bytePos, SEEK_SET);
				string dtime = readDateTimeDouble(FOUT_FILE);

				//write timestep to output
				sysResultsOut << dtime;

				// compute byte distance
				bytePos = RESULTS_OFFSET + (t * BYTES_PER_PERIOD);
				bytePos += DATETIME_BYTES;
				bytePos += SUBCATCHMENT_TIMESTEP_SIZE;
				bytePos += NODE_TIMESTEP_SIZE;
				bytePos += LINK_TIMESTEP_SIZE;
				_fseeki64(FOUT_FILE, bytePos, SEEK_SET);

				// read all of the system variables
				for (size_t i = 0; i < SYSTEM_PAR_COUNT; ++i) {
					sysResultsOut << ",";
					sysResultsOut << readFloat(FOUT_FILE);
					// Add comma
					// end timestep line	
				}
				sysResultsOut << endl;
				sysResultsOut.flush(); // should reduce RAM
			} // end timestep loop
		}


		// Output elements extracted
		cout << '\n' << "Elements Extracted:" << endl;
		cout << "Subcatchments: " << subcatchmentsExtracted << endl;
		cout << "Nodes: " << nodesExtracted << endl;
		cout << "Links: " << linksExtracted << endl;
		cout << "System: " << systemExtracted << endl << '\n';

		// Output runtime
		clock_t end = clock();
		double elapsed_mins = round(double(end - begin) / CLOCKS_PER_SEC / 60 * 100.0) / 100.0;
		//vector SUMMARY_INFO: 0 inNumHead, 1 version, 2 flowunits, 3 errorcode, 4 idNumTail, 5 extractSubcatchments, 6 extractNodes, 7 extractLinks, 8 extractSystem
		cout << "Timesteps: " << NTIMESTEPS << "; Error Code: " << SUMMARY_INFO["errorcode"] << "; ID NUM: " << SUMMARY_INFO["idNumTail"] << endl;
		cout << "Complete, Elapsed Time (min): "; cout << elapsed_mins << endl;

		// generate the file name of log.txt 
		string logFilename = generateFileName(LOG_PATH, i, reportPollutantNames);
		// write the summary information to log.txt
		writeLogTXT(logFilename, elapsed_mins);

		//cin.get();
		fclose(FOUT_FILE);
		cout << endl;
	}
	return 0;
}
