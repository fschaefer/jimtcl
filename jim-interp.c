/*
 * Jim - interp command
 *
 * Copyright 2014 Florian Schäfer <florian.schaefer@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE JIM TCL PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * JIM TCL PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the Jim Tcl Project.
 */

#include <stdlib.h>
#include <string.h>

#include <jim-subcmd.h>

/* From initjimsh.tcl */
extern int Jim_initjimshInit(Jim_Interp *interp);

extern unsigned int Jim_GenHashFunction(const unsigned char *buf, int len);

static unsigned int
JimSlavesHTHashFunction(const void *key)
{
    return Jim_GenHashFunction(key, strlen(key));
}

static void*
JimSlavesHTDup(void *privdata, const void *key)
{
    return Jim_StrDup(key);
}

static int
JimSlavesHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    return strcmp(key1, key2) == 0;
}

static void
JimSlavesHTKeyDestructor(void *privdata, void *key)
{
    Jim_Free(key);
}

static void
JimSlavesHTValDestructor(void *interp, void *val)
{
    Jim_FreeInterp(val);
}

static const Jim_HashTableType JimInterpHashTableType = {
    JimSlavesHTHashFunction,     /* hash function */
    JimSlavesHTDup,              /* key dup */
    NULL,                        /* val dup */
    JimSlavesHTKeyCompare,       /* key compare */
    JimSlavesHTKeyDestructor,    /* key destructor */
    JimSlavesHTValDestructor     /* val destructor */
};

static int
interp_create_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc == 2 && !Jim_CompareStringImmediate(interp, argv[0], "-safe"))
        return JIM_ERR;

    Jim_HashTable *slaves = Jim_GetAssocData(interp, "interp::slaves");

    int save = argc - 1;
    const char *name = Jim_String(argv[0 + save]);
    Jim_HashEntry *he = Jim_FindHashEntry(slaves, name);

    if (he) {
        Jim_DeleteHashEntry(slaves, name);
    }

    Jim_Interp *slave = Jim_CreateInterp();
    Jim_RegisterCoreCommands(slave);
    if (save == 0) {
        Jim_InitStaticExtensions(slave);
        Jim_initjimshInit(slave);
    }

    Jim_AddHashEntry(slaves, name, slave);

    Jim_SetAssocData(slave, "interp::master", NULL, interp);

    Jim_SetResultString(interp, name, -1);
    return JIM_OK;
}

static int
interp_eval_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_HashTable *slaves = Jim_GetAssocData(interp, "interp::slaves");

    const char *name = Jim_String(argv[0]);
    Jim_HashEntry *he = Jim_FindHashEntry(slaves, name);

    if (!he) {
        Jim_SetResultFormatted(interp, "could not find interpreter \"%s\"", name);
        return JIM_ERR;
    }

    Jim_Interp *slave = (Jim_Interp*) he->u.val;

    int rc;
    if (argc == 2) {
        rc = Jim_EvalObj(slave, argv[1]);
    }
    else {
        rc = Jim_EvalObjPrefix(slave, argv[1], argc - 2, argv + 2);
    }

    if (rc == JIM_ERR) {
        /* eval is "interesting", so add a stack frame here */
        slave->addStackTrace++;
    }

    Jim_SetResult(interp, Jim_GetResult(slave));
    return rc;
}

static int
interp_delete_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_HashTable *slaves = Jim_GetAssocData(interp, "interp::slaves");

    const char *name;
    Jim_HashEntry *he;

    int i;
    for (i = 0; i < argc; i++) {
        name = Jim_String(argv[i]);
        he = Jim_FindHashEntry(slaves, name);
        if (!he) {
            Jim_SetResultFormatted(interp, "could not find interpreter \"%s\"", name);
            return JIM_ERR;
        }
        Jim_DeleteHashEntry(slaves, name);
    }

    Jim_SetEmptyResult(interp);
    return JIM_OK;
}

static int
interp_exists_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_HashTable *slaves = Jim_GetAssocData(interp, "interp::slaves");

    const char *name = Jim_String(argv[0]);
    int exists = (Jim_FindHashEntry(slaves, name) ? 1 : 0);

    Jim_SetResult(interp, Jim_NewIntObj(interp, exists));
    return JIM_OK;
}

static int
interp_slaves_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_HashTable *slaves = Jim_GetAssocData(interp, "interp::slaves");

    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

    htiter = Jim_GetHashTableIterator(slaves);
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, he->key, -1));
    }
    Jim_FreeHashTableIterator(htiter);

    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}

static int
interp_slave_alias_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *cmdList;
    Jim_Obj *prefixListObj = Jim_CmdPrivData(interp);
    Jim_Interp *master = Jim_GetAssocData(interp, "interp::master");

    cmdList = Jim_DuplicateObj(master, prefixListObj);
    Jim_ListInsertElements(master, cmdList, Jim_ListLength(master, cmdList), argc - 1, argv + 1);

    int rc;
    rc = Jim_EvalObjList(master, cmdList);

    Jim_SetResult(interp, Jim_GetResult(master));
    return rc;
}

static void
interp_slave_alias_delete(Jim_Interp *interp, void *privData)
{
    Jim_Obj *prefixListObj = privData;
    Jim_DecrRefCount(interp, prefixListObj);
}

static int
interp_alias_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 3) {
        return JIM_ERR;
    }

    Jim_HashTable *slaves = Jim_GetAssocData(interp, "interp::slaves");

    const char *name = Jim_String(argv[0]);
    Jim_HashEntry *he = Jim_FindHashEntry(slaves, name);

    if (!he) {
        Jim_SetResultFormatted(interp, "could not find interpreter \"%s\"", name);
        return JIM_ERR;
    }

    Jim_Interp *slave = (Jim_Interp*)he->u.val;

    Jim_Obj *prefixListObj;
    const char *newname;
    prefixListObj = Jim_NewListObj(slave, argv + 2, argc - 2);
    Jim_IncrRefCount(prefixListObj);
    newname = Jim_String(argv[1]);
    if (newname[0] == ':' && newname[1] == ':') {
        while (*++newname == ':') {
        }
    }

    Jim_SetResult(interp, argv[1]);
    return Jim_CreateCommand(slave, newname, interp_slave_alias_cmd, prefixListObj, interp_slave_alias_delete);
}

static const jim_subcmd_type array_command_table[] = {
    {
        "create",
        "?-safe? name",
        interp_create_cmd,
        1,
        2,
        /* Description: interp create */
    },
    {
        "eval",
        "name arg ?arg ...?",
        interp_eval_cmd,
        2,
        -1,
        /* Description: interp eval */
    },
    {
        "delete",
        "name ?name ...?",
        interp_delete_cmd,
        1,
        -1,
        /* Description: interp delete */
    },
    {
        "exists",
        "name",
        interp_exists_cmd,
        1,
        1,
        /* Description: interp exists */
    },
    {
        "slaves",
        "",
        interp_slaves_cmd,
        0,
        0,
        /* Description: interp slaves */
    },
    {
        "alias",
        "name srcCmd targetCmd ?args ...?",
        interp_alias_cmd,
        3,
        -1,
        /* Description: interp alias */
    },
    {
        NULL
    }
};

static void
interp_free_slaves(Jim_Interp *interp, void *data)
{
    Jim_HashTable *slaves = data;
    Jim_FreeHashTable(slaves);
    Jim_Free(slaves);
}

int
Jim_interpInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "interp", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_HashTable *slaves = Jim_Alloc(sizeof(Jim_HashTable));
    Jim_InitHashTable(slaves, &JimInterpHashTableType, NULL);
    Jim_SetAssocData(interp, "interp::slaves", interp_free_slaves, slaves);

    Jim_CreateCommand(interp, "interp", Jim_SubCmdProc, (void *)array_command_table, NULL);
    return JIM_OK;
}
