%{
/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)svccfg.y	1.8	05/06/10 SMI"

#include <libintl.h>

#include "svccfg.h"

uu_list_pool_t *string_pool;

%}

%union {
	int tok;
	char *str;
	uu_list_t *uul;
}

%start commands

%token SCC_VALIDATE SCC_IMPORT SCC_EXPORT SCC_ARCHIVE SCC_APPLY SCC_EXTRACT
%token SCC_REPOSITORY SCC_INVENTORY SCC_SET SCC_END SCC_HELP
%token SCC_LIST SCC_ADD SCC_DELETE SCC_SELECT SCC_UNSELECT
%token SCC_LISTPG SCC_ADDPG SCC_DELPG
%token SCC_LISTPROP SCC_SETPROP SCC_DELPROP SCC_EDITPROP
%token SCC_ADDPROPVALUE SCC_DELPROPVALUE SCC_SETENV SCC_UNSETENV
%token SCC_LISTSNAP SCC_SELECTSNAP SCC_REVERT
%token SCS_REDIRECT SCS_NEWLINE SCS_EQUALS SCS_LPAREN SCS_RPAREN
%token SCV_WORD SCV_STRING

%type <tok> command_token
%type <str> SCV_WORD SCV_STRING
%type <str> string opt_word
%type <uul> string_list multiline_string_list

%%

/*
 * We could hoist the command terminator for all the rules up here, but then
 * the parser would reduce before shifting the terminator, which would require
 * an additional error rule (per command) to catch extra arguments.
 * This way requires all input to be terminated, which is done by input() in
 * svccfg.l.
 */

commands : command
	| commands command

command : terminator
	| validate_cmd
	| import_cmd
	| export_cmd
	| archive_cmd
	| apply_cmd
	| extract_cmd
	| repository_cmd
	| inventory_cmd
	| set_cmd
	| end_cmd
	| help_cmd
	| list_cmd
	| add_cmd
	| delete_cmd
	| select_cmd
	| unselect_cmd
	| listpg_cmd
	| addpg_cmd
	| delpg_cmd
	| listprop_cmd
	| setprop_cmd
	| delprop_cmd
	| editprop_cmd
	| addpropvalue_cmd
	| delpropvalue_cmd
	| setenv_cmd
	| unsetenv_cmd
	| listsnap_cmd
	| selectsnap_cmd
	| revert_cmd
	| unknown_cmd
	| error terminator	{ semerr(gettext("Syntax error.\n")); }

unknown_cmd : SCV_WORD terminator
	{
		semerr(gettext("Unknown command \"%s\".\n"), $1);
		free($1);
	}
	| SCV_WORD string_list terminator
	{
		string_list_t *slp;
		void *cookie = NULL;

		semerr(gettext("Unknown command \"%s\".\n"), $1);

		while ((slp = uu_list_teardown($2, &cookie)) != NULL) {
			free(slp->str);
			free(slp);
		}

		uu_list_destroy($2);
		free($1);
	}

validate_cmd : SCC_VALIDATE SCV_WORD terminator
	{
		bundle_t *b = internal_bundle_new();
		lxml_get_bundle_file(b, $2, 0);
		(void) internal_bundle_free(b);
		free($2);
	}
	| SCC_VALIDATE error terminator		{ synerr(SCC_VALIDATE); }

import_cmd : SCC_IMPORT string_list terminator
	{
		string_list_t *slp;
		void *cookie = NULL;

		if (engine_import($2) == -2)
			synerr(SCC_IMPORT);

		while ((slp = uu_list_teardown($2, &cookie)) != NULL) {
			free(slp->str);
			free(slp);
		}

		uu_list_destroy($2);
	}
	| SCC_IMPORT error terminator		{ synerr(SCC_IMPORT); }

export_cmd : SCC_EXPORT SCV_WORD terminator
	{
		lscf_service_export($2, NULL);
		free($2);
	}
	| SCC_EXPORT SCV_WORD SCS_REDIRECT SCV_WORD terminator
	{
		lscf_service_export($2, $4);
		free($2);
		free($4);
	}
	| SCC_EXPORT error terminator		{ synerr(SCC_EXPORT); }

archive_cmd : SCC_ARCHIVE terminator
	{
		lscf_archive(NULL);
	}
	| SCC_ARCHIVE SCS_REDIRECT SCV_WORD terminator
	{
		lscf_archive($3);
		free($3);
	}
	| SCC_ARCHIVE error terminator		{ synerr(SCC_ARCHIVE); }

apply_cmd : SCC_APPLY SCV_WORD terminator
					{ (void) engine_apply($2); free($2); }
	| SCC_APPLY error terminator	{ synerr(SCC_APPLY); }

extract_cmd: SCC_EXTRACT terminator	{ lscf_profile_extract(NULL); }
	| SCC_EXTRACT SCS_REDIRECT SCV_WORD terminator
	{
		lscf_profile_extract($3);
		free($3);
	}
	| SCC_EXTRACT error terminator		{ synerr(SCC_EXTRACT); }

repository_cmd : SCC_REPOSITORY SCV_WORD terminator
	{
		lscf_set_repository($2);
		free($2);
	}
	| SCC_REPOSITORY error terminator	{ synerr(SCC_REPOSITORY); }

inventory_cmd : SCC_INVENTORY SCV_WORD terminator
					{ lxml_inventory($2); free($2); }
	| SCC_INVENTORY error terminator	{ synerr(SCC_INVENTORY); }

set_cmd : SCC_SET string_list terminator
	{
		string_list_t *slp;
		void *cookie = NULL;

		(void) engine_set($2);

		while ((slp = uu_list_teardown($2, &cookie)) != NULL) {
			free(slp->str);
			free(slp);
		}

		uu_list_destroy($2);
	}
	| SCC_SET error terminator		{ synerr(SCC_SET); }

end_cmd : SCC_END terminator			{ exit(0); }
	| SCC_END error terminator		{ synerr (SCC_END); }

help_cmd : SCC_HELP terminator			{ help(0); }
	| SCC_HELP command_token terminator	{ help($2); }
	| SCC_HELP error terminator		{ synerr(SCC_HELP); }

list_cmd : SCC_LIST opt_word terminator	{ lscf_list($2); free($2); }
	| SCC_LIST error terminator	{ synerr(SCC_LIST); }

add_cmd : SCC_ADD SCV_WORD terminator	{ lscf_add($2); free($2); }
	| SCC_ADD error terminator	{ synerr(SCC_ADD); }

delete_cmd : SCC_DELETE SCV_WORD terminator
					{ lscf_delete($2, 0); free($2); }
	| SCC_DELETE SCV_WORD SCV_WORD terminator
	{
		if (strcmp($2, "-f") == 0) {
			lscf_delete($3, 1);
			free($2);
			free($3);
		} else {
			synerr(SCC_DELETE);
		}
	}
	| SCC_DELETE error terminator	{ synerr(SCC_DELETE); }

select_cmd : SCC_SELECT SCV_WORD terminator	{ lscf_select($2); free($2); }
	| SCC_SELECT error terminator		{ synerr(SCC_SELECT); }

unselect_cmd : SCC_UNSELECT terminator	{ lscf_unselect(); }
	| SCC_UNSELECT error terminator	{ synerr(SCC_UNSELECT); }

listpg_cmd : SCC_LISTPG opt_word terminator
					{ lscf_listpg($2); free($2); }
	| SCC_LISTPG error terminator	{ synerr(SCC_LISTPG); }

addpg_cmd : SCC_ADDPG SCV_WORD SCV_WORD opt_word terminator
	{
		(void) lscf_addpg($2, $3, $4);
		free($2);
		free($3);
		free($4);
	}
	| SCC_ADDPG error terminator	{ synerr(SCC_ADDPG); }

delpg_cmd : SCC_DELPG SCV_WORD terminator
					{ lscf_delpg($2); free($2); }
	| SCC_DELPG error terminator	{ synerr(SCC_DELPG); }

listprop_cmd : SCC_LISTPROP opt_word terminator
					{ lscf_listprop($2); free($2); }
	| SCC_LISTPROP error terminator	{ synerr(SCC_LISTPROP); }

setprop_cmd : SCC_SETPROP SCV_WORD SCS_EQUALS string terminator
	{
		lscf_setprop($2, NULL, $4, NULL);
		free($2);
		free($4);
	}
	| SCC_SETPROP SCV_WORD SCS_EQUALS SCV_WORD string terminator
	{
		(void) lscf_setprop($2, $4, $5, NULL);
		free($2);
		free($4);
		free($5);
	}
	| SCC_SETPROP SCV_WORD SCS_EQUALS opt_word SCS_LPAREN
	      multiline_string_list SCS_RPAREN terminator
	{
		string_list_t *slp;
		void *cookie = NULL;

		(void) lscf_setprop($2, $4, NULL, $6);

		free($2);
		free($4);

		while ((slp = uu_list_teardown($6, &cookie)) != NULL) {
			free(slp->str);
			free(slp);
		}

		uu_list_destroy($6);
	}
	| SCC_SETPROP error terminator	{ synerr(SCC_SETPROP); }
	| SCC_SETPROP error		{ synerr(SCC_SETPROP); }

delprop_cmd : SCC_DELPROP SCV_WORD terminator
					{ lscf_delprop($2); free($2); }
	| SCC_DELPROP error terminator	{ synerr(SCC_DELPROP); }

editprop_cmd : SCC_EDITPROP terminator	{ lscf_editprop(); }
	| SCC_EDITPROP error terminator	{ synerr(SCC_EDITPROP); }

addpropvalue_cmd : SCC_ADDPROPVALUE SCV_WORD string terminator
	{
		lscf_addpropvalue($2, NULL, $3);
		free($2);
		free($3);
	}
	| SCC_ADDPROPVALUE SCV_WORD string string terminator
	{
		(void) lscf_addpropvalue($2, $3, $4);
		free($2);
		free($3);
		free($4);
	}
	| SCC_ADDPROPVALUE error terminator { synerr(SCC_ADDPROPVALUE); }

delpropvalue_cmd : SCC_DELPROPVALUE SCV_WORD string terminator
	{
		lscf_delpropvalue($2, $3, 0);
		free($2);
		free($3);
	}
	| SCC_DELPROPVALUE error terminator { synerr(SCC_DELPROPVALUE); }

setenv_cmd : SCC_SETENV string_list terminator
	{
		string_list_t *slp;
		void *cookie = NULL;

		if (lscf_setenv($2, 0) == -2)
			synerr(SCC_SETENV);

		while ((slp = uu_list_teardown($2, &cookie)) != NULL) {
			free(slp->str);
			free(slp);
		}

		uu_list_destroy($2);
	}
	| SCC_SETENV error terminator		{ synerr(SCC_SETENV); }

unsetenv_cmd : SCC_UNSETENV string_list terminator
	{
		string_list_t *slp;
		void *cookie = NULL;

		if (lscf_setenv($2, 1) == -2)
			synerr(SCC_UNSETENV);

		while ((slp = uu_list_teardown($2, &cookie)) != NULL) {
			free(slp->str);
			free(slp);
		}

		uu_list_destroy($2);
	}
	| SCC_UNSETENV error terminator		{ synerr(SCC_UNSETENV); }

listsnap_cmd : SCC_LISTSNAP terminator	{ lscf_listsnap(); }
	| SCC_LISTSNAP error terminator	{ synerr(SCC_LISTSNAP); }

selectsnap_cmd : SCC_SELECTSNAP opt_word terminator
					{ lscf_selectsnap($2); free($2); }
	| SCC_SELECTSNAP error terminator
					{ synerr(SCC_SELECTSNAP); }

revert_cmd: SCC_REVERT opt_word terminator	{ lscf_revert($2); free ($2); }
	| SCC_REVERT error terminator		{ synerr(SCC_REVERT); }


terminator : SCS_NEWLINE

string_list :
	{
		$$ = uu_list_create(string_pool, NULL, 0);
		if ($$ == NULL)
			uu_die(gettext("Out of memory\n"));
	}
	| string_list string
	{
		string_list_t *slp;

		slp = safe_malloc(sizeof (*slp));

		slp->str = $2;
		uu_list_node_init(slp, &slp->node, string_pool);
		uu_list_append($1, slp);
		$$ = $1;
	}

multiline_string_list : string_list
	{
		$$ = $1;
	}
	| multiline_string_list SCS_NEWLINE string_list
	{
		void *cookie = NULL;
		string_list_t *slp;

		/* Append $3 to $1. */
		while ((slp = uu_list_teardown($3, &cookie)) != NULL)
			uu_list_append($1, slp);

		uu_list_destroy($3);
	}

string : SCV_WORD	{ $$ = $1; }
	| SCV_STRING	{ $$ = $1; }

opt_word :		{ $$ = NULL; }
	| SCV_WORD	{ $$ = $1; }

command_token : SCC_VALIDATE	{ $$ = SCC_VALIDATE; }
	| SCC_IMPORT		{ $$ = SCC_IMPORT; }
	| SCC_EXPORT		{ $$ = SCC_EXPORT; }
	| SCC_APPLY		{ $$ = SCC_APPLY; }
	| SCC_EXTRACT		{ $$ = SCC_EXTRACT; }
	| SCC_REPOSITORY	{ $$ = SCC_REPOSITORY; }
	| SCC_ARCHIVE		{ $$ = SCC_ARCHIVE; }
	| SCC_INVENTORY		{ $$ = SCC_INVENTORY; }
	| SCC_SET		{ $$ = SCC_SET; }
	| SCC_END		{ $$ = SCC_END; }
	| SCC_HELP		{ $$ = SCC_HELP; }
	| SCC_LIST		{ $$ = SCC_LIST; }
	| SCC_ADD		{ $$ = SCC_ADD; }
	| SCC_DELETE		{ $$ = SCC_DELETE; }
	| SCC_SELECT		{ $$ = SCC_SELECT; }
	| SCC_UNSELECT		{ $$ = SCC_UNSELECT; }
	| SCC_LISTPG		{ $$ = SCC_LISTPG; }
	| SCC_ADDPG		{ $$ = SCC_ADDPG; }
	| SCC_DELPG		{ $$ = SCC_DELPG; }
	| SCC_LISTPROP		{ $$ = SCC_LISTPROP; }
	| SCC_SETPROP		{ $$ = SCC_SETPROP; }
	| SCC_DELPROP		{ $$ = SCC_DELPROP; }
	| SCC_EDITPROP		{ $$ = SCC_EDITPROP; }
	| SCC_ADDPROPVALUE	{ $$ = SCC_ADDPROPVALUE; }
	| SCC_DELPROPVALUE	{ $$ = SCC_DELPROPVALUE; }
	| SCC_SETENV		{ $$ = SCC_SETENV; }
	| SCC_UNSETENV		{ $$ = SCC_UNSETENV; }
	| SCC_LISTSNAP		{ $$ = SCC_LISTSNAP; }
	| SCC_SELECTSNAP	{ $$ = SCC_SELECTSNAP; }
	| SCC_REVERT		{ $$ = SCC_REVERT; }