/*
Source: https://github.com/bitmeal/argv_split
Author: bitmeal
*/

#include <array>
#include <vector>
#include <string>
#include <sstream>

#define ESCAPE_HEX_CODE 0x5c
#define SGLQUOTE_HEX_CODE 0x27
#define DBLQUOTE_HEX_CODE 0x22
#define SPACE_HEX_CODE 0x20

class argv_split {
private:
	bool prependProgname;
	std::string progname;
	std::vector<std::string> arguments;
	std::vector<const char*> argv_arr;

	char quotes[3];
	char space[2];

	bool isEscaped(const std::string& str, size_t idx) {
		// is first character, can not be escaped
		if ( idx == 0 ) return false;
		// is out of bounds (-1)
		if ( idx == std::string::npos ) return false;

		// preceeding character is backslash?
		if ( str.at(idx - 1) == ESCAPE_HEX_CODE )
			return true;
		else
			return false;
	}

	size_t findFirstQuote(const std::string& str) {
		size_t idx = 0;

		do {
			idx = str.find_first_of(quotes, idx == 0 ? 0 : idx + 1);
		} while ( isEscaped( str, idx ) && idx != std::string::npos);

		return idx;
	}

	size_t findMatching(const std::string& str, size_t match_idx) {
		if (match_idx == std::string::npos) return std::string::npos;
		
		char match[1];
		match[0] = str.at( match_idx );

		size_t idx = match_idx;

		do {
			idx = str.find_first_of( match, idx + 1 );
		} while ( isEscaped( str, idx ) && idx != std::string::npos);

		return idx;
	}

	void splitStrToVectorBy(const std::string& str, char delim, std::vector<std::string>& vec)
	{
		std::stringstream strStream( str );
		std::string element;

		while ( std::getline( strStream, element, delim ) )
		{
			if ( !element.empty() )
				vec.push_back( element );
		}
	}

	// function is called recursive
	// inefficient: (but ok for the intended use)
	//	- using substrings (copying) for recursion; C++17 string_view could fix this
	//	- no memory preallocation for arguments vector
	void _parse(const std::string& cmdlinestr) {
		if (cmdlinestr.length() == 0)
			return;

		std::string quoted_str, pre_quoted_str, post_quoted_str;

		// w_front used as limit for string splitting on whitespace
		size_t w_front = cmdlinestr.size(); // if no quotes are found this is used as the substring SIZE for splitting by space
		size_t w_back = cmdlinestr.size() - 1; // if no quotes are found this is used as OFFSET for the next (and final) recursion

		size_t q_front = findFirstQuote(cmdlinestr);
		size_t q_back = findMatching(cmdlinestr, q_front);
		// found an unescaped qoute, are same when npos an so no quote found. are different when two quotes or only a first one are found
		if ( q_front != q_back )
		{			
			// get quoted string
			// and get attached string after quoted string. open quotes will be treated as quotes til the end!

			// found matching quote to first
			if ( q_back != std::string::npos )
			{
				quoted_str = cmdlinestr.substr( q_front + 1, q_back - q_front - 1 );

				// find first whitespace after quote
				w_back = cmdlinestr.find_first_of( space, q_back + 1 );
				// none found? handle as quoted til the end
				if (w_back == std::string::npos)
					w_back = cmdlinestr.size() - 1;

				post_quoted_str = cmdlinestr.substr(q_back + 1, w_back - q_back - 1);
			}
			// did not find matching quote further in string, everything til the end is quoted now
			else
			{
				quoted_str = cmdlinestr.substr(q_front + 1, std::string::npos);
			}


			// get string attached in front of quoted string
			
			// find last whitespace before quote
			w_front = cmdlinestr.find_last_of(space, q_front);
			// none found?
			if ( w_front == std::string::npos )
				w_front = - 1;

			pre_quoted_str = cmdlinestr.substr( w_front + 1, q_front - w_front - 1 );

		}

		// split by whitespace in [0, w_front[
		if(w_front != -1 && w_front != std::string::npos)
			splitStrToVectorBy(cmdlinestr.substr(0, w_front), SPACE_HEX_CODE, arguments);
		
		//add qouted string and surrounding
		if (!quoted_str.empty())
			arguments.push_back(pre_quoted_str + quoted_str + post_quoted_str);
		
		//recurse
		_parse(cmdlinestr.substr(w_back + 1, std::string::npos));
	}

public:
	argv_split() : quotes{ SGLQUOTE_HEX_CODE, DBLQUOTE_HEX_CODE, NULL}, space{ SPACE_HEX_CODE, NULL }, prependProgname(false) {}

	argv_split(std::string progname) : argv_split()
	{
		prependProgname = true;
		this->progname = progname;
	}

	~argv_split() {}

	std::vector<std::string> getArguments()
	{
		return arguments;
	}

	const char** argv() {
		//return argv_arr;
		return argv_arr.data();
	}

	const char** parse(const std::string& cmdline) {
		// setup/cleanup argv pointer
		argv_arr.clear();

		// setup/cleanup arguments
		arguments.clear();
		if (prependProgname)
			arguments.push_back(progname);


		// parse the string and populate arguments vector
		_parse(cmdline);

		// zero arguments? exit now. argv_arr=NULL at this point; allow handling by argv()
		if (arguments.size() == 0)
			return argv();

		// populate argv array
		for (auto& it : arguments)
			argv_arr.push_back(it.c_str());
		// terminate with null as standard argv array
		argv_arr.push_back(NULL);

		return argv();
	}
};