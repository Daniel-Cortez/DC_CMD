#define	DC_CMD_VERSION	"2.02"

#include <stddef.h>

#include "SDK/amx/amx.h"
#include "SDK/plugincommon.h"
typedef void (*logprintf_t)(char* format, ...);
extern void *pAMXFunctions;
logprintf_t logprintf;

#include "MurmurHash3.cpp"
#define Murmur3(src,len,dest) MurmurHash3_x86_32(src,len,0,dest)

#ifdef _MSC_VER
    // resolving type conflicts by shitcoding =/
    #define	uint8_t uint8_t_redef
    #define	int32_t int32_t_redef
    #define	uint32_t uint32_t_redef
#endif
#include <boost/unordered_map.hpp>
#ifdef _MSC_VER
    #undef	uint32_t
    #undef	int32_t
    #undef	uint8_t
#endif

#define MAX_AMX 1+16
struct DC_CMD_AMX_Funcs {AMX* amx; int OPCR, OPCP;} amx_List[MAX_AMX];
int lastAMX = 0;
boost::unordered_map <int, int> Alts[MAX_AMX];
size_t Alts_n = 0;

cell AMX_NATIVE_CALL amx_DC_CMD(AMX* amx, cell* params)
{
	cell *addr;
	int len;
	amx_GetAddr(amx, params[2], &addr);
	amx_StrLen(addr, &len);
	if(len>127) len=127;
	++len;
	char cmdtext[128];
	amx_GetString(cmdtext, addr, 0, len);
	cmdtext[0] = '_';
	// converting string to lower case
	int pos=0, cmd_end;
	do{
		++pos;
		if(('A' <= cmdtext[pos]) && (cmdtext[pos] <= 'Z'))
			cmdtext[pos] += ('a'-'A');
		else if(cmdtext[pos] == '\0')
			break;
		else if(cmdtext[pos] == ' ')
		{
			cmd_end = pos;
			cmdtext[pos++] = '\0';
			goto loop1_exit;
		}
	}while(1);
	cmd_end = 0;
loop1_exit:
	// search for command index in all AMX instances
	int pubidx;
	cell retval, params_addr;
	int i;
	for(i=0; i<=lastAMX; ++i)
	{
		if((amx_List[i].amx != NULL) && (amx_FindPublic(amx_List[i].amx, cmdtext, &pubidx) == AMX_ERR_NONE))
		{
			// if current AMX instance has OnPlayerCommandReceived callback - invoke it
			if(amx_List[i].OPCR != 0x7FFFFFFF)
			{
				// restore some symbols in cmdtext
				cmdtext[0] = '/';
				if(cmd_end>0)
					cmdtext[cmd_end] = ' ';
				amx_PushString(amx_List[i].amx, &params_addr, 0, cmdtext, 0, 0);
				amx_Push(amx_List[i].amx, params[1]);
				amx_Exec(amx_List[i].amx, &retval, amx_List[i].OPCR);
				amx_Release(amx_List[i].amx, params_addr);
				// if OPCR returned 0 - command execution rejected
				if(retval == 0)
					return 1;
				cmdtext[0] = '_';	// restore AMX-styled command name
				if(cmd_end>0)		// and separate it from parameters (again =/)
					cmdtext[cmd_end] = ' ';
			}
			// remove extra space characters between command name and parameters
			while(cmdtext[pos] == ' ') pos++;
			amx_PushString(amx_List[i].amx, &params_addr, 0, cmdtext+pos, 0, 0);
			amx_Push(amx_List[i].amx, params[1]);
			amx_Exec(amx_List[i].amx, &retval, pubidx);
			amx_Release(amx_List[i].amx, params_addr);
			// if current AMX instance has OnPlayerCommandPerformed callback - invoke it
			if(amx_List[i].OPCP != 0x7FFFFFFF)
			{
				cmdtext[0] = '/';
				if(cmd_end>0)
					cmdtext[cmd_end] = ' ';
				amx_Push(amx_List[i].amx, retval);
				amx_PushString(amx_List[i].amx, &params_addr, 0, cmdtext, 0, 0);
				amx_Push(amx_List[i].amx, params[1]);
				amx_Exec(amx_List[i].amx, &retval, amx_List[i].OPCP);
				amx_Release(amx_List[i].amx, params_addr);
			}
			return 1;
		}
	}
	// if command wasn't found - perhaps this is an alternative command
	if(Alts_n != 0)
	{
		int hash;
		// remove extra space characters between command name and parameters
		//logprintf("attempting to find alt %s, len = %d", cmdtext, (cmdtext[pos])?(pos-1):(pos));
		Murmur3(cmdtext, (cmdtext[pos])?(pos-1):(pos), &hash);
		if(cmdtext[pos])
		{
			pos--;
			while(cmdtext[++pos] == ' '){}
		}
		//logprintf((char*)"Murmur3(%s) = 0x%X", cmdtext, hash);
		boost::unordered_map<int,int>::const_iterator alt;
		for(i=0; i<=lastAMX; ++i)
		{
			if((amx_List[i].amx != NULL) && ((alt = Alts[i].find(hash)) != Alts[i].end()))
			{
				pubidx = alt->second;
				//logprintf("found alt: %s, amx = %d, idx = %d", cmdtext, (int)amx, pubidx);
				if(amx_List[i].OPCR != 0x7FFFFFFF)
				{
					// restore some symbols in cmdtext
					cmdtext[0] = '/';
					if(cmd_end>0)
						cmdtext[cmd_end] = ' ';
					amx_PushString(amx_List[i].amx, &params_addr, 0, cmdtext, 0, 0);
					amx_Push(amx_List[i].amx, params[1]);
					amx_Exec(amx_List[i].amx, &retval, amx_List[i].OPCR);
					amx_Release(amx_List[i].amx, params_addr);
					// if OPCR returned 0 - command execution rejected
					if(retval == 0)
						return 1;
					cmdtext[0] = '_';	// restore AMX-styled command name
					if(cmd_end>0)		// and separate it from parameters (again =/)
						cmdtext[cmd_end] = ' ';
				}
				// remove extra space characters between command name and parameters
				while(cmdtext[pos] == ' ') pos++;
				amx_PushString(amx_List[i].amx, &params_addr, 0, cmdtext+pos, 0, 0);
				amx_Push(amx_List[i].amx, params[1]);
				amx_Exec(amx_List[i].amx, &retval, pubidx);
				amx_Release(amx_List[i].amx, params_addr);
				// if current AMX instance has OnPlayerCommandPerformed callback - invoke it
				if(amx_List[i].OPCP != 0x7FFFFFFF)
				{
					cmdtext[0] = '/';
					if(cmd_end>0)
						cmdtext[cmd_end] = ' ';
					amx_Push(amx_List[i].amx, retval);
					amx_PushString(amx_List[i].amx, &params_addr, 0, cmdtext, 0, 0);
					amx_Push(amx_List[i].amx, params[1]);
					amx_Exec(amx_List[i].amx, &retval, amx_List[i].OPCP);
					amx_Release(amx_List[i].amx, params_addr);
				}
				return 1;
			}
		}
	}
	// if command not found - call OnPlayerCommandPerformed callback in gamemode AMX (success = -1)
	if(amx_List[0].OPCP != 0x7FFFFFFF)
	{
		cmdtext[0] = '/';
		if(cmd_end>0)
			cmdtext[cmd_end] = ' ';
		amx_Push(amx_List[0].amx, -1);
		amx_PushString(amx_List[0].amx, &params_addr, 0, cmdtext, 0, 0);
		amx_Push(amx_List[0].amx, params[1]);
		amx_Exec(amx_List[0].amx, &retval, amx_List[0].OPCP);
		amx_Release(amx_List[0].amx, params_addr);
	}
	return 1;
}

cell AMX_NATIVE_CALL amx_RegisterAlt(AMX* amx, cell* params)
{
	int amx_n;
	for(amx_n=0; amx_n<=lastAMX; ++amx_n)
		if(amx == amx_List[amx_n].amx)
			break;
	if(amx_n>lastAMX) // if amx wasn't found in list
		return 0;
	cell *addr;
	int len;
	amx_GetAddr(amx, params[1], &addr);
	amx_StrLen(addr, &len);
	if(len>31) len=31;
	++len;
	char cmd[32];
	amx_GetString(cmd, addr, 0, len);
	cmd[0] = '_';
	// converting string to lower case
	int pos=0;
	do{
		++pos;
		if(('A' <= cmd[pos]) && (cmd[pos] <= 'Z'))
			cmd[pos] += ('a'-'A');
		else if(cmd[pos] == '\0')
			break;
		else if((cmd[pos] == ' ') || (cmd[pos] == '\t'))
		{
			cmd[pos] = '\0';
			break;
		}
	}while(1);
	int pubidx;
	if(amx_FindPublic(amx, cmd, &pubidx) != AMX_ERR_NONE)
	{
		//logprintf((char*)"RegisterAlt: Couldn't find function %s", cmd);
		return 1;
	}
	int alt_n = (params[0]/4), hash;
	//logprintf("RegisterAlt: alts = %d", alt_n-1);
	do{
		if(amx_GetAddr(amx, params[alt_n], &addr) != AMX_ERR_NONE)
			continue;
		amx_StrLen(addr, &len);
		if(len>31) len=31; // command length must be up to 31 chars
		++len;
		amx_GetString(cmd, addr, 0, len);
		cmd[0] = '_';
		pos = 0;
		do{
			++pos;
			if(('A' <= cmd[pos]) && (cmd[pos] <= 'Z'))
				cmd[pos] += ('a'-'A');
			else if(cmd[pos] == '\0')
				break;
			else if((cmd[pos] == ' ') || (cmd[pos] == '\t'))
			{
				cmd[pos] = '\0';
				break;
			}
		}while(1);
		//logprintf((char*)"%s, len = %d", cmd, pos);
		Murmur3(cmd, pos, &hash);
		//logprintf("RegisterAlt: Murmur3(%s) = 0x%X", cmd, hash);
		Alts[amx_n].insert(std::make_pair(hash, pubidx));
		Alts_n++;
		//logprintf((char*)"RegisterAlt: new alt - %s, amx = %d, pubidx = %d", cmd, amx, pubidx);
	}while(--alt_n > 1);
	return 1;
}


AMX_NATIVE_INFO PluginNatives[] =
{
    {"DC_CMD", amx_DC_CMD},
	{"RegisterAlt", amx_RegisterAlt},
    {0, 0}
};

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData)
{
	pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
	logprintf = (logprintf_t) ppData[PLUGIN_DATA_LOGPRINTF];
	logprintf((char*)"  Daniel's CMD plugin v"DC_CMD_VERSION);
	return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
	logprintf((char*)"  Daniel's CMD plugin got unloaded");
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX *amx)
{
	int idx;
	if(amx_List[0].amx == NULL && amx_FindPublic(amx, "OnGameModeInit", &idx) == AMX_ERR_NONE)
		idx = 0;		// if it's a gamemode AMX - make it first in list
	else
		idx = ++lastAMX;// else - put it to the end of list
	amx_List[idx].amx = amx;
	amx_FindPublic(amx, "OnPlayerCommandReceived", &amx_List[idx].OPCR);
	amx_FindPublic(amx, "OnPlayerCommandPerformed", &amx_List[idx].OPCP);
	return amx_Register(amx, PluginNatives, -1);
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX *amx)
{
	int idx = 0;
	if(amx == amx_List[0].amx)	// if unloaded AMX is a gamemode
		amx_List[0].amx = NULL;
	else
	{							// move last AMX in list to position of unloaded AMX
		for(idx=1; idx<MAX_AMX; ++idx)
			if(amx_List[idx].amx == amx)
			{
				if(lastAMX > 1)
					amx_List[idx].amx = amx_List[lastAMX].amx;
				amx_List[lastAMX].amx = NULL;
				--lastAMX;
				break;
			}
	}
	Alts_n -= Alts[idx].size();
	Alts[idx].clear();
    return AMX_ERR_NONE;
}
