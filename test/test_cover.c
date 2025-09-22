//
//  Copyright (C) 2025  Nick Gasson
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "test_util.h"
#include "cov/cov-api.h"
#include "cov/cov-priv.h"
#include "cov/cov-structs.h"
#include "ident.h"
#include "jit/jit.h"
#include "lib.h"
#include "option.h"
#include "phase.h"
#include "rt/model.h"
#include "scan.h"
#include "tree.h"

#include <limits.h>

static cover_data_t *run_cover(void)
{
   cover_data_t *db = cover_data_init(COVER_MASK_ALL, 0, 1);
   mir_context_t *mc = get_mir();
   unit_registry_t *ur = get_registry();
   jit_t *j = jit_new(ur, mc, db);

   tree_t t, last_ent = NULL;
   while ((t = parse())) {
      fail_if(error_count() > 0);

      lib_put(lib_work(), t);
      simplify_local(t, j, ur, mc);
      bounds_check(t);
      fail_if(error_count() > 0);

      const tree_kind_t kind = tree_kind(t);
      if (kind == T_ENTITY || kind == T_CONFIGURATION)
         last_ent = t;
   }

   rt_model_t *m = model_new(j, db);

   elab(tree_to_object(last_ent), j, ur, mc, db, NULL, m);

   model_reset(m);
   model_run(m, UINT64_MAX);

   model_free(m);
   jit_free(j);

   return db;
}

START_TEST(test_perfile1)
{
   input_from_file(TESTDIR "/cover/perfile1.vhd");

   cover_data_t *db = run_cover();

   // TODO: shouldn't need to do this to sync counters
   fbuf_t *tmp = fbuf_open("/tmp/perfile1.ncdb", FBUF_OUT, FBUF_CS_NONE);
   cover_dump_items(db, tmp, COV_DUMP_RUNTIME);
   fbuf_close(tmp, NULL);

   cover_rpt_t *rpt = cover_report_new(db, INT_MAX);

   cover_scope_t *u1 = cover_get_scope(db, ident_new("WORK.TOP.U1"));
   ck_assert_ptr_nonnull(u1);

   const rpt_hier_t *u1_h = rpt_get_hier(rpt, u1);
   ck_assert_int_eq(u1_h->flat_stats.total[COV_ITEM_STMT], 3);
   ck_assert_int_eq(u1_h->flat_stats.hit[COV_ITEM_STMT], 2);

   const rpt_file_t *f = rpt_get_file(rpt, u1);
   ck_assert_ptr_nonnull(f);

   ck_assert_int_eq(f->stats.total[COV_ITEM_STMT], 3);
   ck_assert_int_eq(f->stats.hit[COV_ITEM_STMT], 3);

   cover_report_free(rpt);
   cover_data_free(db);

   fail_if_errors();
}
END_TEST

Suite *get_cover_tests(void)
{
   Suite *s = suite_create("cover");

   TCase *tc = nvc_unit_test();
   tcase_add_test(tc, test_perfile1);
   suite_add_tcase(s, tc);

   return s;
}
