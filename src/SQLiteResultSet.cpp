#include "SQLiteResultSet.h"
#include "sqlite3.h"
#include <co/Coral.h>
#include "DBException.h"
#include <string>
using namespace std;

namespace ca
{
	
	bool SQLiteResultSet::next()
	{
		int status = sqlite3_step(_stmt);
		if(status == SQLITE_ERROR)
		{
			throw ca::DBException("error on getting next result on ResultSet");
		}
		if(status == SQLITE_DONE)
		{
			return false;
		}
		return true;
	}
	
	const std::string& SQLiteResultSet::getValue( co::int32 columnIndex)
	{
		_value = (char*)sqlite3_column_text(_stmt, columnIndex);
		return _value;
	}

	void SQLiteResultSet::setStatement(sqlite3_stmt* stmt)
	{ 
		_stmt = stmt; 
	}

	void SQLiteResultSet::finalize()
	{
		if( _stmt )
		{
			int error = sqlite3_finalize(_stmt);

			if( error != SQLITE_OK )
			{
				throw ca::DBException("Could not finalize ResultSet");
			}

		}
		_stmt = 0;
	}

};