/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "g_local.h"

// this file is only included when building a dll
// g_syscalls.asm is included instead when building a qvm
#ifdef Q3_VM
#error "Do not use in VM build"
#endif

static intptr_t (QDECL *g_syscall)( intptr_t arg, ... ) = (intptr_t (QDECL *)( intptr_t, ...))-1;

// lgodlewski: the mutex has to be recursive, because engine→VM→engine→VM...
// callstacks happen quite often
#include <tbb/tbb.h>
static tbb::recursive_mutex g_syscallMutex;
static tbb::mutex g_printMutex;
static tbb::mutex g_FSMutex;
static tbb::mutex g_cvarMutex;

Q_EXPORT void dllEntry( intptr_t (QDECL *syscallptr)( intptr_t arg,... ) ) {
	g_syscall = syscallptr;
}

int PASSFLOAT( float x ) {
	floatint_t fi;
	fi.f = x;
	return fi.i;
}

void	trap_Print( const char *text ) {
	tbb::mutex::scoped_lock lock(g_printMutex);	// lgodlewski
	g_syscall( G_PRINT, text );
}

void trap_Error( const char *text )
{
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_ERROR, text );
	// shut up GCC warning about returning functions, because we know better
	exit(1);
}

int		trap_Milliseconds( void ) {
	// lgodlewski: no mutex here, Sys_Milliseconds() is thread-safe
	return g_syscall( G_MILLISECONDS );
}
int		trap_Argc( void ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( G_ARGC );
}

void	trap_Argv( int n, char *buffer, int bufferLength ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_ARGV, n, buffer, bufferLength );
}

int		trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	tbb::mutex::scoped_lock lock(g_FSMutex);	// lgodlewski
	return g_syscall( G_FS_FOPEN_FILE, qpath, f, mode );
}

void	trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	tbb::mutex::scoped_lock lock(g_FSMutex);	// lgodlewski
	g_syscall( G_FS_READ, buffer, len, f );
}

void	trap_FS_Write( const void *buffer, int len, fileHandle_t f ) {
	tbb::mutex::scoped_lock lock(g_FSMutex);	// lgodlewski
	g_syscall( G_FS_WRITE, buffer, len, f );
}

void	trap_FS_FCloseFile( fileHandle_t f ) {
	tbb::mutex::scoped_lock lock(g_FSMutex);	// lgodlewski
	g_syscall( G_FS_FCLOSE_FILE, f );
}

int trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize ) {
	tbb::mutex::scoped_lock lock(g_FSMutex);	// lgodlewski
	return g_syscall( G_FS_GETFILELIST, path, extension, listbuf, bufsize );
}

int trap_FS_Seek( fileHandle_t f, long offset, int origin ) {
	tbb::mutex::scoped_lock lock(g_FSMutex);	// lgodlewski
	return g_syscall( G_FS_SEEK, f, offset, origin );
}

void	trap_SendConsoleCommand( int exec_when, const char *text ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_SEND_CONSOLE_COMMAND, exec_when, text );
}

void	trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags ) {
	tbb::mutex::scoped_lock lock(g_cvarMutex);	// lgodlewski
	g_syscall( G_CVAR_REGISTER, cvar, var_name, value, flags );
}

void	trap_Cvar_Update( vmCvar_t *cvar ) {
	tbb::mutex::scoped_lock lock(g_cvarMutex);	// lgodlewski
	g_syscall( G_CVAR_UPDATE, cvar );
}

void trap_Cvar_Set( const char *var_name, const char *value ) {
	tbb::mutex::scoped_lock lock(g_cvarMutex);	// lgodlewski
	g_syscall( G_CVAR_SET, var_name, value );
}

int trap_Cvar_VariableIntegerValue( const char *var_name ) {
	tbb::mutex::scoped_lock lock(g_cvarMutex);	// lgodlewski
	return g_syscall( G_CVAR_VARIABLE_INTEGER_VALUE, var_name );
}

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	tbb::mutex::scoped_lock lock(g_cvarMutex);	// lgodlewski
	g_syscall( G_CVAR_VARIABLE_STRING_BUFFER, var_name, buffer, bufsize );
}


void trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
						 playerState_t *clients, int sizeofGClient ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_LOCATE_GAME_DATA, gEnts, numGEntities, sizeofGEntity_t, clients, sizeofGClient );
}

void trap_DropClient( int clientNum, const char *reason ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_DROP_CLIENT, clientNum, reason );
}

void trap_SendServerCommand( int clientNum, const char *text ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_SEND_SERVER_COMMAND, clientNum, text );
}

void trap_SetConfigstring( int num, const char *string ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_SET_CONFIGSTRING, num, string );
}

void trap_GetConfigstring( int num, char *buffer, int bufferSize ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_GET_CONFIGSTRING, num, buffer, bufferSize );
}

void trap_GetUserinfo( int num, char *buffer, int bufferSize ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_GET_USERINFO, num, buffer, bufferSize );
}

void trap_SetUserinfo( int num, const char *buffer ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_SET_USERINFO, num, buffer );
}

void trap_GetServerinfo( char *buffer, int bufferSize ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_GET_SERVERINFO, buffer, bufferSize );
}

void trap_SetBrushModel( gentity_t *ent, const char *name ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_SET_BRUSH_MODEL, ent, name );
}

void trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_TRACE, results, start, mins, maxs, end, passEntityNum, contentmask );
}

void trap_TraceCapsule( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_TRACECAPSULE, results, start, mins, maxs, end, passEntityNum, contentmask );
}

int trap_PointContents( const vec3_t point, int passEntityNum ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( G_POINT_CONTENTS, point, passEntityNum );
}


qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return (qboolean)g_syscall( G_IN_PVS, p1, p2 );
}

qboolean trap_InPVSIgnorePortals( const vec3_t p1, const vec3_t p2 ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return (qboolean)g_syscall( G_IN_PVS_IGNORE_PORTALS, p1, p2 );
}

void trap_AdjustAreaPortalState( gentity_t *ent, qboolean open ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_ADJUST_AREA_PORTAL_STATE, ent, open );
}

qboolean trap_AreasConnected( int area1, int area2 ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return (qboolean)g_syscall( G_AREAS_CONNECTED, area1, area2 );
}

void trap_LinkEntity( gentity_t *ent ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_LINKENTITY, ent );
}

void trap_UnlinkEntity( gentity_t *ent ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_UNLINKENTITY, ent );
}

int trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *list, int maxcount ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( G_ENTITIES_IN_BOX, mins, maxs, list, maxcount );
}

qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return (qboolean)g_syscall( G_ENTITY_CONTACT, mins, maxs, ent );
}

qboolean trap_EntityContactCapsule( const vec3_t mins, const vec3_t maxs, const gentity_t *ent ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return (qboolean)g_syscall( G_ENTITY_CONTACTCAPSULE, mins, maxs, ent );
}

int trap_BotAllocateClient( void ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( G_BOT_ALLOCATE_CLIENT );
}

void trap_BotFreeClient( int clientNum ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_BOT_FREE_CLIENT, clientNum );
}

void trap_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_GET_USERCMD, clientNum, cmd );
}

qboolean trap_GetEntityToken( char *buffer, int bufferSize ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return (qboolean)g_syscall( G_GET_ENTITY_TOKEN, buffer, bufferSize );
}

int trap_DebugPolygonCreate(int color, int numPoints, vec3_t *points) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( G_DEBUG_POLYGON_CREATE, color, numPoints, points );
}

void trap_DebugPolygonDelete(int id) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_DEBUG_POLYGON_DELETE, id );
}

int trap_RealTime( qtime_t *qtime ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( G_REAL_TIME, qtime );
}

void trap_SnapVector( float *v ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( G_SNAPVECTOR, v );
}

// BotLib traps start here
int trap_BotLibSetup( void ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_SETUP );
}

int trap_BotLibShutdown( void ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_SHUTDOWN );
}

int trap_BotLibVarSet(char *var_name, char *value) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_LIBVAR_SET, var_name, value );
}

int trap_BotLibVarGet(char *var_name, char *value, int size) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_LIBVAR_GET, var_name, value, size );
}

int trap_BotLibDefine(char *string) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_PC_ADD_GLOBAL_DEFINE, string );
}

int trap_BotLibStartFrame(float time) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_START_FRAME, PASSFLOAT( time ) );
}

int trap_BotLibLoadMap(const char *mapname) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_LOAD_MAP, mapname );
}

int trap_BotLibUpdateEntity(int ent, void /* struct bot_updateentity_s */ *bue) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_UPDATENTITY, ent, bue );
}

int trap_BotLibTest(int parm0, char *parm1, vec3_t parm2, vec3_t parm3) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_TEST, parm0, parm1, parm2, parm3 );
}

int trap_BotGetSnapshotEntity( int clientNum, int sequence ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_GET_SNAPSHOT_ENTITY, clientNum, sequence );
}

int trap_BotGetServerCommand(int clientNum, char *message, int size) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_GET_CONSOLE_MESSAGE, clientNum, message, size );
}

void trap_BotUserCommand(int clientNum, usercmd_t *ucmd) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_USER_COMMAND, clientNum, ucmd );
}

void trap_AAS_EntityInfo(int entnum, void /* struct aas_entityinfo_s */ *info) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AAS_ENTITY_INFO, entnum, info );
}

int trap_AAS_Initialized(void) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_INITIALIZED );
}

void trap_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX, presencetype, mins, maxs );
}

float trap_AAS_Time(void) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	floatint_t fi;
	fi.i = g_syscall( BOTLIB_AAS_TIME );
	return fi.f;
}

int trap_AAS_PointAreaNum(vec3_t point) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_POINT_AREA_NUM, point );
}

int trap_AAS_PointReachabilityAreaIndex(vec3_t point) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX, point );
}

int trap_AAS_TraceAreas(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_TRACE_AREAS, start, end, areas, points, maxareas );
}

int trap_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_BBOX_AREAS, absmins, absmaxs, areas, maxareas );
}

int trap_AAS_AreaInfo( int areanum, void /* struct aas_areainfo_s */ *info ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_AREA_INFO, areanum, info );
}

int trap_AAS_PointContents(vec3_t point) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_POINT_CONTENTS, point );
}

int trap_AAS_NextBSPEntity(int ent) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_NEXT_BSP_ENTITY, ent );
}

int trap_AAS_ValueForBSPEpairKey(int ent, char *key, char *value, int size) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY, ent, key, value, size );
}

int trap_AAS_VectorForBSPEpairKey(int ent, char *key, vec3_t v) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY, ent, key, v );
}

int trap_AAS_FloatForBSPEpairKey(int ent, char *key, float *value) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY, ent, key, value );
}

int trap_AAS_IntForBSPEpairKey(int ent, char *key, int *value) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY, ent, key, value );
}

int trap_AAS_AreaReachability(int areanum) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_AREA_REACHABILITY, areanum );
}

int trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA, areanum, origin, goalareanum, travelflags );
}

int trap_AAS_EnableRoutingArea( int areanum, int enable ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_ENABLE_ROUTING_AREA, areanum, enable );
}

int trap_AAS_PredictRoute(void /*struct aas_predictroute_s*/ *route, int areanum, vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_PREDICT_ROUTE, route, areanum, origin, goalareanum, travelflags, maxareas, maxtime, stopevent, stopcontents, stoptfl, stopareanum );
}

int trap_AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags,
										void /*struct aas_altroutegoal_s*/ *altroutegoals, int maxaltroutegoals,
										int type) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL, start, startareanum, goal, goalareanum, travelflags, altroutegoals, maxaltroutegoals, type );
}

int trap_AAS_Swimming(vec3_t origin) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_SWIMMING, origin );
}

int trap_AAS_PredictClientMovement(void /* struct aas_clientmove_s */ *move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT, move, entnum, origin, presencetype, onground, velocity, cmdmove, cmdframes, maxframes, PASSFLOAT(frametime), stopevent, stopareanum, visualize );
}

void trap_EA_Say(int client, char *str) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_SAY, client, str );
}

void trap_EA_SayTeam(int client, char *str) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_SAY_TEAM, client, str );
}

void trap_EA_Command(int client, char *command) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_COMMAND, client, command );
}

void trap_EA_Action(int client, int action) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_ACTION, client, action );
}

void trap_EA_Gesture(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_GESTURE, client );
}

void trap_EA_Talk(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_TALK, client );
}

void trap_EA_Attack(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_ATTACK, client );
}

void trap_EA_Use(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_USE, client );
}

void trap_EA_Respawn(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_RESPAWN, client );
}

void trap_EA_Crouch(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_CROUCH, client );
}

void trap_EA_MoveUp(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_MOVE_UP, client );
}

void trap_EA_MoveDown(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_MOVE_DOWN, client );
}

void trap_EA_MoveForward(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_MOVE_FORWARD, client );
}

void trap_EA_MoveBack(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_MOVE_BACK, client );
}

void trap_EA_MoveLeft(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_MOVE_LEFT, client );
}

void trap_EA_MoveRight(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_MOVE_RIGHT, client );
}

void trap_EA_SelectWeapon(int client, int weapon) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_SELECT_WEAPON, client, weapon );
}

void trap_EA_Jump(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_JUMP, client );
}

void trap_EA_DelayedJump(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_DELAYED_JUMP, client );
}

void trap_EA_Move(int client, vec3_t dir, float speed) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_MOVE, client, dir, PASSFLOAT(speed) );
}

void trap_EA_View(int client, vec3_t viewangles) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_VIEW, client, viewangles );
}

void trap_EA_EndRegular(int client, float thinktime) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_END_REGULAR, client, PASSFLOAT(thinktime) );
}

void trap_EA_GetInput(int client, float thinktime, void /* struct bot_input_s */ *input) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_GET_INPUT, client, PASSFLOAT(thinktime), input );
}

void trap_EA_ResetInput(int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_EA_RESET_INPUT, client );
}

int trap_BotLoadCharacter(char *charfile, float skill) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_LOAD_CHARACTER, charfile, PASSFLOAT(skill));
}

void trap_BotFreeCharacter(int character) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_FREE_CHARACTER, character );
}

float trap_Characteristic_Float(int character, int index) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	floatint_t fi;
	fi.i = g_syscall( BOTLIB_AI_CHARACTERISTIC_FLOAT, character, index );
	return fi.f;
}

float trap_Characteristic_BFloat(int character, int index, float min, float max) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	floatint_t fi;
	fi.i = g_syscall( BOTLIB_AI_CHARACTERISTIC_BFLOAT, character, index, PASSFLOAT(min), PASSFLOAT(max) );
	return fi.f;
}

int trap_Characteristic_Integer(int character, int index) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_CHARACTERISTIC_INTEGER, character, index );
}

int trap_Characteristic_BInteger(int character, int index, int min, int max) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_CHARACTERISTIC_BINTEGER, character, index, min, max );
}

void trap_Characteristic_String(int character, int index, char *buf, int size) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_CHARACTERISTIC_STRING, character, index, buf, size );
}

int trap_BotAllocChatState(void) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_ALLOC_CHAT_STATE );
}

void trap_BotFreeChatState(int handle) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_FREE_CHAT_STATE, handle );
}

void trap_BotQueueConsoleMessage(int chatstate, int type, char *message) {
	tbb::mutex::scoped_lock lock(g_printMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_QUEUE_CONSOLE_MESSAGE, chatstate, type, message );
}

void trap_BotRemoveConsoleMessage(int chatstate, int handle) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_REMOVE_CONSOLE_MESSAGE, chatstate, handle );
}

int trap_BotNextConsoleMessage(int chatstate, void /* struct bot_consolemessage_s */ *cm) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_NEXT_CONSOLE_MESSAGE, chatstate, cm );
}

int trap_BotNumConsoleMessages(int chatstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_NUM_CONSOLE_MESSAGE, chatstate );
}

void trap_BotInitialChat(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_INITIAL_CHAT, chatstate, type, mcontext, var0, var1, var2, var3, var4, var5, var6, var7 );
}

int	trap_BotNumInitialChats(int chatstate, char *type) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_NUM_INITIAL_CHATS, chatstate, type );
}

int trap_BotReplyChat(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_REPLY_CHAT, chatstate, message, mcontext, vcontext, var0, var1, var2, var3, var4, var5, var6, var7 );
}

int trap_BotChatLength(int chatstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_CHAT_LENGTH, chatstate );
}

void trap_BotEnterChat(int chatstate, int client, int sendto) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_ENTER_CHAT, chatstate, client, sendto );
}

void trap_BotGetChatMessage(int chatstate, char *buf, int size) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_GET_CHAT_MESSAGE, chatstate, buf, size);
}

int trap_StringContains(char *str1, char *str2, int casesensitive) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_STRING_CONTAINS, str1, str2, casesensitive );
}

int trap_BotFindMatch(char *str, void /* struct bot_match_s */ *match, unsigned long int context) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_FIND_MATCH, str, match, context );
}

void trap_BotMatchVariable(void /* struct bot_match_s */ *match, int variable, char *buf, int size) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_MATCH_VARIABLE, match, variable, buf, size );
}

void trap_UnifyWhiteSpaces(char *string) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_UNIFY_WHITE_SPACES, string );
}

void trap_BotReplaceSynonyms(char *string, unsigned long int context) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_REPLACE_SYNONYMS, string, context );
}

int trap_BotLoadChatFile(int chatstate, char *chatfile, char *chatname) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_LOAD_CHAT_FILE, chatstate, chatfile, chatname );
}

void trap_BotSetChatGender(int chatstate, int gender) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_SET_CHAT_GENDER, chatstate, gender );
}

void trap_BotSetChatName(int chatstate, char *name, int client) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_SET_CHAT_NAME, chatstate, name, client );
}

void trap_BotResetGoalState(int goalstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_RESET_GOAL_STATE, goalstate );
}

void trap_BotResetAvoidGoals(int goalstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_RESET_AVOID_GOALS, goalstate );
}

void trap_BotRemoveFromAvoidGoals(int goalstate, int number) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_REMOVE_FROM_AVOID_GOALS, goalstate, number);
}

void trap_BotPushGoal(int goalstate, void /* struct bot_goal_s */ *goal) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_PUSH_GOAL, goalstate, goal );
}

void trap_BotPopGoal(int goalstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_POP_GOAL, goalstate );
}

void trap_BotEmptyGoalStack(int goalstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_EMPTY_GOAL_STACK, goalstate );
}

void trap_BotDumpAvoidGoals(int goalstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_DUMP_AVOID_GOALS, goalstate );
}

void trap_BotDumpGoalStack(int goalstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_DUMP_GOAL_STACK, goalstate );
}

void trap_BotGoalName(int number, char *name, int size) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_GOAL_NAME, number, name, size );
}

int trap_BotGetTopGoal(int goalstate, void /* struct bot_goal_s */ *goal) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_GET_TOP_GOAL, goalstate, goal );
}

int trap_BotGetSecondGoal(int goalstate, void /* struct bot_goal_s */ *goal) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_GET_SECOND_GOAL, goalstate, goal );
}

int trap_BotChooseLTGItem(int goalstate, vec3_t origin, int *inventory, int travelflags) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_CHOOSE_LTG_ITEM, goalstate, origin, inventory, travelflags );
}

int trap_BotChooseNBGItem(int goalstate, vec3_t origin, int *inventory, int travelflags, void /* struct bot_goal_s */ *ltg, float maxtime) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_CHOOSE_NBG_ITEM, goalstate, origin, inventory, travelflags, ltg, PASSFLOAT(maxtime) );
}

int trap_BotTouchingGoal(vec3_t origin, void /* struct bot_goal_s */ *goal) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_TOUCHING_GOAL, origin, goal );
}

int trap_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void /* struct bot_goal_s */ *goal) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE, viewer, eye, viewangles, goal );
}

int trap_BotGetLevelItemGoal(int index, char *classname, void /* struct bot_goal_s */ *goal) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_GET_LEVEL_ITEM_GOAL, index, classname, goal );
}

int trap_BotGetNextCampSpotGoal(int num, void /* struct bot_goal_s */ *goal) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL, num, goal );
}

int trap_BotGetMapLocationGoal(char *name, void /* struct bot_goal_s */ *goal) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_GET_MAP_LOCATION_GOAL, name, goal );
}

float trap_BotAvoidGoalTime(int goalstate, int number) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	floatint_t fi;
	fi.i = g_syscall( BOTLIB_AI_AVOID_GOAL_TIME, goalstate, number );
	return fi.f;
}

void trap_BotSetAvoidGoalTime(int goalstate, int number, float avoidtime) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_SET_AVOID_GOAL_TIME, goalstate, number, PASSFLOAT(avoidtime));
}

void trap_BotInitLevelItems(void) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_INIT_LEVEL_ITEMS );
}

void trap_BotUpdateEntityItems(void) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_UPDATE_ENTITY_ITEMS );
}

int trap_BotLoadItemWeights(int goalstate, char *filename) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_LOAD_ITEM_WEIGHTS, goalstate, filename );
}

void trap_BotFreeItemWeights(int goalstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_FREE_ITEM_WEIGHTS, goalstate );
}

void trap_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC, parent1, parent2, child );
}

void trap_BotSaveGoalFuzzyLogic(int goalstate, char *filename) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC, goalstate, filename );
}

void trap_BotMutateGoalFuzzyLogic(int goalstate, float range) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC, goalstate, PASSFLOAT(range) );
}

int trap_BotAllocGoalState(int state) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_ALLOC_GOAL_STATE, state );
}

void trap_BotFreeGoalState(int handle) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_FREE_GOAL_STATE, handle );
}

void trap_BotResetMoveState(int movestate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_RESET_MOVE_STATE, movestate );
}

void trap_BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_ADD_AVOID_SPOT, movestate, origin, PASSFLOAT(radius), type);
}

void trap_BotMoveToGoal(void /* struct bot_moveresult_s */ *result, int movestate, void /* struct bot_goal_s */ *goal, int travelflags) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_MOVE_TO_GOAL, result, movestate, goal, travelflags );
}

int trap_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_MOVE_IN_DIRECTION, movestate, dir, PASSFLOAT(speed), type );
}

void trap_BotResetAvoidReach(int movestate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_RESET_AVOID_REACH, movestate );
}

void trap_BotResetLastAvoidReach(int movestate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_RESET_LAST_AVOID_REACH,movestate  );
}

int trap_BotReachabilityArea(vec3_t origin, int testground) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_REACHABILITY_AREA, origin, testground );
}

int trap_BotMovementViewTarget(int movestate, void /* struct bot_goal_s */ *goal, int travelflags, float lookahead, vec3_t target) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_MOVEMENT_VIEW_TARGET, movestate, goal, travelflags, PASSFLOAT(lookahead), target );
}

int trap_BotPredictVisiblePosition(vec3_t origin, int areanum, void /* struct bot_goal_s */ *goal, int travelflags, vec3_t target) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_PREDICT_VISIBLE_POSITION, origin, areanum, goal, travelflags, target );
}

int trap_BotAllocMoveState(void) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_ALLOC_MOVE_STATE );
}

void trap_BotFreeMoveState(int handle) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_FREE_MOVE_STATE, handle );
}

void trap_BotInitMoveState(int handle, void /* struct bot_initmove_s */ *initmove) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_INIT_MOVE_STATE, handle, initmove );
}

int trap_BotChooseBestFightWeapon(int weaponstate, int *inventory) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON, weaponstate, inventory );
}

void trap_BotGetWeaponInfo(int weaponstate, int weapon, void /* struct weaponinfo_s */ *weaponinfo) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_GET_WEAPON_INFO, weaponstate, weapon, weaponinfo );
}

int trap_BotLoadWeaponWeights(int weaponstate, char *filename) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_LOAD_WEAPON_WEIGHTS, weaponstate, filename );
}

int trap_BotAllocWeaponState(void) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_ALLOC_WEAPON_STATE );
}

void trap_BotFreeWeaponState(int weaponstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_FREE_WEAPON_STATE, weaponstate );
}

void trap_BotResetWeaponState(int weaponstate) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	g_syscall( BOTLIB_AI_RESET_WEAPON_STATE, weaponstate );
}

int trap_GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION, numranks, ranks, parent1, parent2, child );
}

int trap_PC_LoadSource( const char *filename ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_PC_LOAD_SOURCE, filename );
}

int trap_PC_FreeSource( int handle ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_PC_FREE_SOURCE, handle );
}

int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_PC_READ_TOKEN, handle, pc_token );
}

int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	//tbb::recursive_mutex::scoped_lock lock(g_syscallMutex);	// lgodlewski
	return g_syscall( BOTLIB_PC_SOURCE_FILE_AND_LINE, handle, filename, line );
}
