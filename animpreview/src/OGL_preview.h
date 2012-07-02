#ifndef INCLUDED_OGL_PREVIEW_H
#define INCLUDED_OGL_PREVIEW_H
#define MAX_32BIT_INT 2147483647

//http://stackoverflow.com/questions/865668/parse-command-line-arguments:
char* get_argument(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool get_option(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}


// https://github.com/imageworks/pystring
// Why this is *.cpp/*.h? Shouldn't it be header-only?
void 
split_whitespace( const std::string & str, std::vector< std::string > & result, int maxsplit )
{
    std::string::size_type i, j, len = str.size();
    for (i = j = 0; i < len; )
    {
	    while ( i < len && ::isspace( str[i] ) ) i++;
	    j = i;
	    while ( i < len && ! ::isspace( str[i]) ) i++;
	    if (j < i)
	    {
		    if ( maxsplit-- <= 0 ) break;
		    result.push_back( str.substr( j, i - j ));
		    while ( i < len && ::isspace( str[i])) i++;
		    j = i;
	    }
    }
    if (j < len)
    {
	    result.push_back( str.substr( j, len - j ));
    }
}


void 
split( const std::string & str, std::vector< std::string > & result, const std::string & sep, int maxsplit )
{
    result.clear();
    if ( maxsplit < 0 ) maxsplit = MAX_32BIT_INT;
    if ( sep.size() == 0 )
    {
        split_whitespace( str, result, maxsplit );
        return;
    }
    std::string::size_type i,j, len = str.size(), n = sep.size();
    i = j = 0;
    while ( i+n <= len )
    {
        if ( str[i] == sep[0] && str.substr( i, n ) == sep )
        {
            if ( maxsplit-- <= 0 ) break;

            result.push_back( str.substr( j, i - j ) );
            i = j = i + n;
        }
        else
        {
            i++;
        }
    }
    result.push_back( str.substr( j, len-j ) );
}
#endif

