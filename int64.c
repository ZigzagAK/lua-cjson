#include <lua.h>
#include <lauxlib.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM==501

/* Compatibility for Lua 5.1 and older LuaJIT.
 *
 * luaL_setfuncs() is used to create a module table where the functions have
 * json_config_t as their first upvalue. Code borrowed from Lua 5.2 source. */
static void luaL_setfuncs (lua_State *l, const luaL_Reg *reg, int nup)
{
    int i;

    luaL_checkstack(l, nup, "too many upvalues");
    for (; reg->name != NULL; reg++) {  /* fill the table with given functions */
        for (i = 0; i < nup; i++)  /* copy upvalues to the top */
            lua_pushvalue(l, -nup);
        lua_pushcclosure(l, reg->func, nup);  /* closure with those upvalues */
        lua_setfield(l, -(nup + 2), reg->name);
    }
    lua_pop(l, nup);  /* remove upvalues */
}

static void *luaL_testudata (lua_State *L, int i, const char *tname) {
  void *p = lua_touserdata(L, i);
  luaL_checkstack(L, 2, "not enough stack slots");
  if (p == NULL || !lua_getmetatable(L, i))
    return NULL;
  else {
    int res = 0;
    luaL_getmetatable(L, tname);
    res = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    if (!res)
      p = NULL;
  }
  return p;
}


#define luaL_newlibtable(L,l) (lua_createtable(L,0,sizeof(l)))
#define luaL_newlib(L,l) (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

#endif

static const char *INT64  = "INT64";
static const char *UINT64 = "UINT64";

#include "int64.h"


struct struct64_s {
    const char *type;
    union {
        int64_t  i;
        uint64_t u;
    } data;
};
typedef struct struct64_s struct64_t;


static int
init_INT64(lua_State *L, struct64_t *val)
{
    int type = lua_type(L, 1);

    switch (type) {
        case LUA_TNUMBER:
            val->data.i = (int64_t) lua_tointeger(L, 1);
            break;
        case LUA_TSTRING: {
            size_t len = 0;
            const char *str = (const char *)lua_tolstring(L, 1, &len);
            char *endptr;
            val->data.i = (int64_t) strtoll(str, &endptr, 10);
            if ((errno == ERANGE && (val->data.i == LLONG_MAX || val->data.i == LLONG_MIN))
                       || (errno != 0 && val->data.i == 0)) {
                return luaL_error(L, "The string (length = %d) is not an int64 string", len);
            }
            break;
        }
        case LUA_TUSERDATA: {
            struct64_t *p = (struct64_t *)luaL_testudata(L, 1, "INT64");
            if (p != NULL) {
                val->data.i = p->data.i;
                break;
            }
            p = luaL_testudata(L, 1, "UINT64");
            if (p != NULL) {
                val->data.i = p->data.u;
                break;
            }
        }
        default:
            return luaL_error(L, "argument error type %s", lua_typename(L, type));
    }

    return 1;
}

static int
init_UINT64(lua_State *L, struct64_t *val)
{
    int type = lua_type(L, 1);

    switch (type) {
        case LUA_TNUMBER:
            val->data.u = (int64_t) lua_tointeger(L, 1);
            break;
        case LUA_TSTRING: {
            size_t len = 0;
            const char *str = (const char *)lua_tolstring(L, 1, &len);
            char *endptr;
            val->data.u = (uint64_t) strtoull(str, &endptr, 10);
            if ((errno == ERANGE && (val->data.u == ULLONG_MAX || val->data.u == 0))
                       || (errno != 0 && val->data.u == 0)) {
                return luaL_error(L, "The string (length = %d) is not an int64 string", len);
            }
            break;
        }
        case LUA_TUSERDATA: {	
            struct64_t *p = (struct64_t *)luaL_checkudata(L, 1, "INT64");
            if (p == NULL) {
                p = luaL_checkudata(L, 1, "UINT64");
                if (p == NULL) {
                    return luaL_error(L, "invalid argument");
                }
                val->data.u = p->data.u;
                break;
            } else {
                val->data.u = p->data.i;
                break;
            }
            break;
        }
        default:
            return luaL_error(L, "argument error type %s", lua_typename(L, type));
    }

    return 1;
}


static struct64_t*
new(lua_State *L, const char *type)
{
    struct64_t *r = (struct64_t *) lua_newuserdata(L, sizeof(struct64_t));
    if (r == NULL) {
        return NULL;
    }

    r->type = type;

    luaL_getmetatable(L, type);
    lua_setmetatable(L, -2);

    return r;
}


int
push_i64(lua_State *L, int64_t v)
{
    struct64_t *r = new(L, INT64);
    if (r == NULL) {
        return luaL_error(L, "no memory");
    }
    r->data.i = v;
    return 1;
}


int
push_u64(lua_State *L, uint64_t v)
{
    struct64_t *r = new(L, UINT64);
    if (r == NULL) {
        return luaL_error(L, "no memory");
    }
    r->data.u = v;
    return 1;
}


int64_t
get_i64(void *ud)
{
    return ud ? ((struct64_t *) ud)->data.i : 0;
}


uint64_t
get_u64(void *ud)
{
    return ud ? ((struct64_t *) ud)->data.u : 0;
}


static int
new_signed(lua_State *L)
{
    int         n = lua_gettop(L);
    struct64_t *r = new(L, INT64);
    if (r == NULL) {
        return luaL_error(L, "no memory");
    }
    if (n == 0) {
        r->data.u = 0;
        return 1;
    }
    return init_INT64(L, r);
}


static int
new_unsigned(lua_State *L)
{
    int         n = lua_gettop(L);
    struct64_t *r = new(L, UINT64);
    if (r == NULL) {
        return luaL_error(L, "no memory");
    }
    if (n == 0) {
        r->data.i = 0;
        return 1;
    }
    return init_UINT64(L, r);
}


static struct64_t*
get_userdata(lua_State *L, int index)
{
    struct64_t *v = (struct64_t *)luaL_testudata(L, index, "UINT64");
    if (v != NULL) {
        return v;
    }
    return (struct64_t *)luaL_checkudata(L, index, "INT64");
}


#define DECLARE_METHOD(M, TYPE, D, OP)                   \
static int                                               \
_## M ##_## TYPE (lua_State *L)                          \
{                                                        \
    struct64_t *a;                                       \
    struct64_t *b;                                       \
    struct64_t *result;                                  \
                                                         \
    if (lua_gettop(L) != 2) {                            \
        return luaL_error(L, "two arguments required");  \
    }                                                    \
                                                         \
    result = new(L, TYPE);                               \
    if (result == NULL) {                                \
        return luaL_error(L, "no memory");               \
    }                                                    \
                                                         \
    a = get_userdata(L, 1);                              \
    b = get_userdata(L, 2);                              \
    if (a == NULL || b == NULL) {                        \
        return luaL_error(L, "invalid argument");        \
    }                                                    \
                                                         \
    result->data.D = (a->type == INT64 ? a->data.i       \
                                        : a->data.u)     \
        OP (b->type == INT64 ? b->data.i : b->data.u);   \
                                                         \
    return 1;                                            \
}


DECLARE_METHOD(add, INT64, i, +)
DECLARE_METHOD(sub, INT64, i, -)
DECLARE_METHOD(mul, INT64, i, *)
DECLARE_METHOD(div, INT64, i, /)
DECLARE_METHOD(mod, INT64, i, %)


DECLARE_METHOD(add, UINT64, u, +)
DECLARE_METHOD(sub, UINT64, u, -)
DECLARE_METHOD(mul, UINT64, u, *)
DECLARE_METHOD(div, UINT64, u, /)
DECLARE_METHOD(mod, UINT64, u, %)


#define DECLARE_POW(TYPE, D)                            \
static int                                              \
pow_ ## TYPE (lua_State *L)                             \
{                                                       \
    struct64_t *a;                                      \
    struct64_t *b;                                      \
    struct64_t *result;                                 \
                                                        \
    if (lua_gettop(L) != 2) {                           \
        return luaL_error(L, "two arguments required"); \
    }                                                   \
                                                        \
    result = new(L, TYPE);                              \
    if (result == NULL) {                               \
        return luaL_error(L, "no memory");              \
    }                                                   \
                                                        \
    a = get_userdata(L, 1);                             \
    b = get_userdata(L, 2);                             \
    if (a == NULL || b == NULL) {                       \
        return luaL_error(L, "invalid argument");       \
    }                                                   \
                                                        \
    result->data.D = pow(a->type == INT64 ? a->data.i   \
                                          : a->data.u,  \
                         b->type == INT64 ? b->data.i   \
                                          : b->data.u   \
    );                                                  \
                                                        \
    return 1;                                           \
}


DECLARE_POW(INT64, i)
DECLARE_POW(UINT64, u)


static int
unm(lua_State *L)
{
    struct64_t *a;
    struct64_t *result;

    if (lua_gettop(L) != 1) {
        return luaL_error(L, "one argument required");
    }

    result = new(L, INT64);
    if (result == NULL) {
        return luaL_error(L, "no memory");
    }

    a = luaL_checkudata(L, 1, "INT64");
    if (a == NULL) {
        return luaL_error(L, "invalid argument");
    }

    result->data.i = -a->data.i;

    return 1;
}


#define DECLARE_CMP(M, TYPE, OP)                        \
static int                                              \
_## M ##_## TYPE (lua_State *L)                         \
{                                                       \
    struct64_t *a;                                      \
    struct64_t *b;                                      \
                                                        \
    if (lua_gettop(L) != 2) {                           \
        return luaL_error(L, "two arguments required"); \
    }                                                   \
                                                        \
    a = get_userdata(L, 1);                             \
    b = get_userdata(L, 2);                             \
    if (a == NULL || b == NULL) {                       \
        return luaL_error(L, "invalid argument");       \
    }                                                   \
                                                        \
    lua_pushboolean(L,                                  \
      (a->type == INT64 ? a->data.i : a->data.u)        \
            OP                                          \
      (b->type == INT64 ? b->data.i : b->data.u)        \
    );                                                  \
                                                        \
    return 1;                                           \
}


DECLARE_CMP(eq, INT64, ==)
DECLARE_CMP(lt, INT64, <)
DECLARE_CMP(le, INT64, <=)

DECLARE_CMP(eq, UINT64, ==)
DECLARE_CMP(lt, UINT64, <)
DECLARE_CMP(le, UINT64, <=)


static int
len(lua_State *L) {
    lua_pushnumber(L, sizeof(ptrdiff_t));
    return 1;
}


#define DECLARE_TOSTRING(TYPE, D, FMT)                  \
static int                                              \
tostring_ ## TYPE (lua_State *L)                        \
{                                                       \
    struct64_t *a;                                      \
    char buffer[23];                                    \
    int len;                                            \
                                                        \
    if (lua_gettop(L) != 1) {                           \
        return luaL_error(L, "one argument required");  \
    }                                                   \
                                                        \
    a = (struct64_t *)luaL_checkudata(L, 1, #TYPE);     \
    if (a == NULL) {                                    \
        return luaL_error(L, "invalid argument");       \
    }                                                   \
                                                        \
    len = snprintf(buffer, 22, "%" FMT, a->data.D);     \
    if (len < 0) {                                      \
        return luaL_error(L, "invalid argument");       \
    }                                                   \
    lua_pushlstring(L, (const char *)buffer, len);      \
                                                        \
    return 1;                                           \
}


DECLARE_TOSTRING(INT64, i, PRId64)
DECLARE_TOSTRING(UINT64, u, PRIu64)


static luaL_Reg
lib_int64[] = {
    { "__add", _add_INT64 },
    { "__sub", _sub_INT64 },
    { "__mul", _mul_INT64 },
    { "__div", _div_INT64 },
    { "__mod", _mod_INT64 },
    { "__unm", unm },
    { "__pow", pow_INT64 },
    { "__eq", _eq_INT64 },
    { "__lt", _lt_INT64 },
    { "__le", _le_INT64 },
    { "__len", len },
    { "__tostring", tostring_INT64 },
    { NULL, NULL },
};


static luaL_Reg
lib_uint64[] = {
    { "__add", _add_UINT64 },
    { "__sub", _sub_UINT64 },
    { "__mul", _mul_UINT64 },
    { "__div", _div_UINT64 },
    { "__mod", _mod_UINT64 },
    { "__unm", NULL },
    { "__pow", pow_UINT64 },
    { "__eq", _eq_UINT64 },
    { "__lt", _lt_UINT64 },
    { "__le", _le_UINT64 },
    { "__len", len },
    { "__tostring", tostring_UINT64 },
    { NULL, NULL },
};


static const luaL_Reg
funcs[] = {
    { "signed",   new_signed },
    { "unsigned", new_unsigned },
    { NULL,  NULL }
};

int
luaopen_int64(lua_State *L) {
    luaL_newmetatable(L, INT64);
    luaL_setfuncs(L, lib_int64, 0);
    lua_setfield(L, -1, "__index");

    luaL_newmetatable(L, UINT64);
    luaL_setfuncs(L, lib_uint64, 0);
    lua_setfield(L, -1, "__index");

    luaL_newlib(L, funcs);

    return 1;
}

