//
//  Copyright (C) 2023-2025  Nick Gasson
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

#include "util.h"
#include "common.h"
#include "cov/cov-api.h"
#include "lib.h"
#include "lower.h"
#include "option.h"
#include "psl/psl-fsm.h"
#include "psl/psl-node.h"
#include "psl/psl-phase.h"
#include "tree.h"
#include "type.h"
#include "vcode.h"

#include <assert.h>
#include <stdlib.h>

#define PSL_BLOCK_CASE  1
#define PSL_BLOCK_ABORT 2
#define PSL_BLOCK_PREV  3

static void psl_wait_cb(tree_t t, void *ctx)
{
   lower_unit_t *lu = ctx;

   vcode_reg_t nets_reg = lower_lvalue(lu, t);
   vcode_reg_t count_reg = emit_const(vtype_offset(), type_width(tree_type(t)));
   emit_sched_event(nets_reg, count_reg);
}

static vcode_reg_t psl_lower_boolean(lower_unit_t *lu, psl_node_t p)
{
   assert(psl_kind(p) == P_HDL_EXPR);
   vcode_reg_t test_reg = lower_rvalue(lu, psl_tree(p));

   if (!vtype_eq(vcode_reg_type(test_reg), vtype_bool())) {
      vcode_type_t std_logic = vtype_int(0, 8);
      vcode_reg_t one_reg = emit_const(std_logic, 3);
      return emit_cmp(VCODE_CMP_EQ, test_reg, one_reg);
   }
   else
      return test_reg;
}

static vcode_reg_t psl_debug_locus(psl_node_t p)
{
   ident_t unit;
   ptrdiff_t offset;
   psl_locus(p, &unit, &offset);

   return emit_debug_locus(unit, offset);
}

static vcode_reg_t psl_assert_severity(void)
{
   return emit_const(vtype_int(0, 3), 2);
}

static vcode_reg_t psl_lower_guard(lower_unit_t *lu, psl_guard_t g)
{
   switch (psl_guard_kind(g)) {
   case GUARD_EXPR:
      return psl_lower_boolean(lu, psl_guard_expr(g));
   case GUARD_BINOP:
      {
         const guard_binop_t *bop = psl_guard_binop(g);
         vcode_reg_t left_reg = psl_lower_guard(lu, bop->left);
         vcode_reg_t right_reg = psl_lower_guard(lu, bop->right);

         switch (bop->kind) {
         case BINOP_AND:
            return emit_and(left_reg, right_reg);
         case BINOP_OR:
            return emit_or(left_reg, right_reg);
         default:
            should_not_reach_here();
         }
      }
   case GUARD_NOT:
      return emit_not(psl_lower_boolean(lu, psl_guard_expr(g)));
   default:
      should_not_reach_here();
   }
}

static void psl_lower_cover(lower_unit_t *lu, psl_node_t p, cover_data_t *cover,
                            cover_scope_t *cscope)
{
   if (psl_has_message(p)) {
      tree_t m = psl_message(p);
      vcode_reg_t msg_reg = lower_rvalue(lu, m);

      vcode_type_t voffset = vtype_offset();
      vcode_reg_t count_reg = emit_const(voffset, type_width(tree_type(m)));

      vcode_reg_t severity_reg = emit_const(vtype_int(0, 3), 0);
      vcode_reg_t locus = psl_debug_locus(p);

      emit_report(msg_reg, count_reg, severity_reg, locus);
   }

   if (!cover_enabled(cover, COVER_MASK_FUNCTIONAL))
      return;

   cover_item_t *item = cover_add_items_for(cover, cscope, psl_to_object(p),
                                            COV_ITEM_FUNCTIONAL);
   if (item == NULL)
      return;

   emit_cover_stmt(item->tag);
}

static void psl_enter_state(fsm_state_t *state)
{
   vcode_reg_t strong_reg = VCODE_INVALID_REG;
   if (state->strong)
      strong_reg = emit_const(vtype_bool(), 1);

   vcode_type_t vint32 = vtype_int(INT32_MIN, INT32_MAX);
   emit_enter_state(emit_const(vint32, state->id), strong_reg);
}

static void psl_lower_state(lower_unit_t *lu, psl_fsm_t *fsm,
                            fsm_state_t *state, vcode_block_t *state_bb,
                            cover_data_t *cover, cover_scope_t *cscope)
{
   emit_comment("Property state %d", state->id);

   if (state->initial && psl_fsm_repeating(fsm))
      psl_enter_state(state);

   vcode_type_t vbool = vtype_bool();
   vcode_reg_t vfalse = emit_const(vbool, 0);
   vcode_reg_t vtrue = emit_const(vbool, 1);

   if (state->accept) {
      vcode_block_t cont_bb = vcode_active_block();
      if (state->guard != NULL) {
         vcode_block_t accept_bb = emit_block();
         cont_bb = emit_block();

         vcode_reg_t guard_reg = psl_lower_guard(lu, state->guard);
         emit_cond(guard_reg, accept_bb, cont_bb);

         vcode_select_block(accept_bb);
      }

      if (fsm->kind == FSM_COVER)
         psl_lower_cover(lu, fsm->src, cover, cscope);
      else if (fsm->kind == FSM_NEVER) {
         vcode_reg_t severity_reg = psl_assert_severity();
         vcode_reg_t locus = psl_debug_locus(fsm->src);

         emit_assert(vfalse, VCODE_INVALID_REG, VCODE_INVALID_REG,
                     severity_reg, locus, VCODE_INVALID_REG,
                     VCODE_INVALID_REG);
      }

      emit_return(VCODE_INVALID_REG);

      if (state->guard == NULL)
         return;
      else
         vcode_select_block(cont_bb);
   }

   vcode_reg_t taken_reg = vfalse;

   for (fsm_edge_t *e = state->edges; e; e = e->next) {
      assert(e->kind == EDGE_NEXT);

      if (e->guard != NULL) {
         vcode_reg_t guard_reg = psl_lower_guard(lu, e->guard);

         vcode_block_t enter_bb = emit_block();
         vcode_block_t skip_bb = emit_block();
         emit_cond(guard_reg, enter_bb, skip_bb);

         vcode_select_block(enter_bb);
         psl_enter_state(e->dest);
         emit_jump(skip_bb);

         vcode_select_block(skip_bb);

         taken_reg = emit_or(taken_reg, guard_reg);
      }
      else {
         psl_enter_state(e->dest);
         taken_reg = vtrue;
      }
   }

   if (fsm->kind != FSM_COVER && fsm->kind != FSM_NEVER) {
      vcode_reg_t severity_reg = psl_assert_severity();
      vcode_reg_t locus = psl_debug_locus(state->where);

      emit_assert(taken_reg, VCODE_INVALID_REG, VCODE_INVALID_REG,
                  severity_reg, locus, VCODE_INVALID_REG,
                  VCODE_INVALID_REG);
   }

   emit_return(VCODE_INVALID_REG);
}

static psl_node_t psl_outer_async_abort(psl_node_t p)
{
   switch (psl_kind(p)) {
   case P_ABORT:
      if (psl_subkind(p) == PSL_ABORT_ASYNC)
         return psl_operand(p, 1);
      else
         return NULL;

   case P_ALWAYS:
   case P_NEVER:
   case P_ASSUME:
   case P_COVER:
   case P_CLOCKED:
      return psl_outer_async_abort(psl_value(p));

   default:
      return NULL;
   }
}

static vcode_reg_t psl_lower_async_abort(unit_registry_t *ur,
                                         lower_unit_t *parent,
                                         psl_node_t hdl_expr,
                                         ident_t name_prefix)
{
   vcode_state_t state;
   vcode_state_save(&state);

   vcode_unit_t context = get_vcode(parent);
   vcode_select_unit(context);

   ident_t prefix = vcode_unit_name(context);
   ident_t name = ident_prefix(name_prefix, ident_new("async_abort"), '$');

   vcode_unit_t vu = emit_function(name, psl_to_object(hdl_expr), context);
   vcode_set_result(vtype_bool());

   vcode_type_t vcontext = vtype_context(prefix);
   emit_param(vcontext, vcontext, ident_new("context"));

   lower_unit_t *lu = lower_unit_new(ur, parent, vu, NULL, NULL);
   unit_registry_put(ur, lu);

   vcode_reg_t result_reg = psl_lower_boolean(lu, hdl_expr);
   emit_return(result_reg);

   unit_registry_finalise(ur, lu);

   vcode_state_restore(&state);

   vcode_reg_t context_reg = emit_context_upref(1);
   vcode_reg_t args[] = { context_reg };
   return emit_function_trigger(name, args, ARRAY_LEN(args));
}

vcode_reg_t psl_lower_fcall(lower_unit_t *lu, psl_node_t p)
{
   assert(psl_kind(p) == P_BUILTIN_FCALL);

   if (psl_subkind(p) != PSL_BUILTIN_PREV)
      fatal_at(psl_loc(p), "sorry, this built-in function is not supported");

   vcode_state_t state;
   vcode_state_save(&state);

   vcode_select_block(PSL_BLOCK_PREV);

   tree_t expr = psl_tree(psl_operand(p, 0));
   type_t type = tree_type(expr);

   int num = 1;
   if (psl_operands(p) > 1)
      num = assume_int(psl_tree(psl_operand(p, 1)));

   if (num > 512)
      fatal_at(psl_loc(p), "sorry, Number higher than 512 is not supported");

   vcode_type_t vtype = lower_type(type);
   vcode_type_t vbounds = lower_bounds(type);

   vcode_var_t *vars LOCAL = xmalloc_array(num, sizeof(vcode_var_t));
   for (int i = 0; i < num; i++)
      vars[i] = emit_var(vtype, vbounds, ident_uniq("prev"), 0);

   vcode_reg_t cur_reg = lower_rvalue(lu, expr);
   vcode_reg_t count_reg = VCODE_INVALID_REG;

   const bool is_array = type_is_array(type);
   if (is_array) {
      int64_t length;
      if (!folded_length(range_of(type, 0), &length))
         fatal_at(psl_loc(p), "sorry, only constant length arrays "
                  "are supported");

      count_reg = emit_const(vtype_offset(), length);
   }

   for (int i = 0; i < num - 1; i++) {
      if (is_array) {
         vcode_reg_t src_ptr = emit_index(vars[i + 1], VCODE_INVALID_REG);
         vcode_reg_t dst_ptr = emit_index(vars[i], VCODE_INVALID_REG);
         emit_copy(dst_ptr, src_ptr, count_reg);
      }
      else {
         vcode_reg_t tmp = emit_load(vars[i + 1]);
         emit_store(tmp, vars[i]);
      }
   }

   if (is_array) {
      vcode_reg_t dst_ptr = emit_index(vars[num - 1], VCODE_INVALID_REG);
      emit_copy(dst_ptr, cur_reg, count_reg);
   }
   else
      emit_store(cur_reg, vars[num - 1]);

   vcode_state_restore(&state);

   if (is_array)
      return emit_index(vars[0], VCODE_INVALID_REG);
   else
      return emit_load(vars[0]);
}

void psl_lower_directive(unit_registry_t *ur, lower_unit_t *parent,
                         cover_data_t *cover, tree_t wrapper)
{
   psl_node_t p = tree_psl(wrapper);
   ident_t label = tree_ident(wrapper);

   cover_scope_t *parent_cscope = lower_get_cover_scope(parent);
   cover_scope_t *cscope =
      cover_create_scope(cover, parent_cscope, wrapper, NULL);

   psl_fsm_t *fsm = psl_fsm_new(p, label);

   vcode_unit_t context = get_vcode(parent);

   ident_t prefix = vcode_unit_name(context);
   ident_t name = ident_prefix(prefix, label, '.');

   vcode_unit_t vu = emit_property(name, psl_to_object(p), context);
   lower_unit_t *lu = lower_unit_new(ur, parent, vu, NULL, NULL);
   unit_registry_put(ur, lu);

   vcode_type_t vcontext = vtype_context(prefix);
   emit_param(vcontext, vcontext, ident_new("context"));

   vcode_type_t vint32 = vtype_int(INT32_MIN, INT32_MAX);
   vcode_reg_t state_reg = emit_param(vint32, vint32, ident_new("state"));

   vcode_block_t case_bb = emit_block();
   vcode_block_t abort_bb = emit_block();
   vcode_block_t prev_bb = emit_block();

   assert(case_bb == PSL_BLOCK_CASE);
   assert(abort_bb == PSL_BLOCK_ABORT);
   assert(prev_bb == PSL_BLOCK_PREV);

   // Only handle a single clock for the whole property
   psl_node_t top = psl_value(p);
   assert(psl_kind(top) == P_CLOCKED);
   assert(psl_has_ref(top));

   psl_node_t clk = psl_ref(top);
   tree_t clk_expr = psl_tree(psl_ref(top));

   int hops;
   vcode_var_t trigger_var = lower_search_vcode_obj(clk, lu, &hops);
   assert(trigger_var != VCODE_INVALID_VAR);

   emit_comment("Reset property");

   build_wait(clk_expr, psl_wait_cb, lu);

   vcode_reg_t trigger_ptr = emit_var_upref(hops, trigger_var);
   vcode_reg_t trigger_reg = emit_load_indirect(trigger_ptr);

   // Only handle async_abort at the top level
   psl_node_t async_abort = psl_outer_async_abort(top);
   if (async_abort != NULL) {
      build_wait(psl_tree(async_abort), psl_wait_cb, lu);

      vcode_reg_t abort_reg =
         psl_lower_async_abort(ur, parent, async_abort, name);
      trigger_reg = emit_or_trigger(trigger_reg, abort_reg);
   }

   emit_add_trigger(trigger_reg);
   emit_jump(prev_bb);

   vcode_select_block(case_bb);

   vcode_block_t *state_bb LOCAL =
      xmalloc_array(fsm->next_id + 1, sizeof(vcode_block_t));
   vcode_reg_t *state_ids LOCAL =
      xmalloc_array(fsm->next_id + 1, sizeof(vcode_reg_t));

   for (int i = 0; i < fsm->next_id; i++) {
      state_bb[i] = emit_block();
      state_ids[i] = emit_const(vint32, i);
   }

   state_bb[fsm->next_id] = prev_bb;
   state_ids[fsm->next_id] = emit_const(vint32, fsm->next_id);

   bool strong = false;
   for (fsm_state_t *s = fsm->states; s; s = s->next) {
      vcode_select_block(state_bb[s->id]);
      psl_lower_state(lu, fsm, s, state_bb, cover, cscope);
      strong |= s->strong;
   }

   vcode_select_block(abort_bb);

   if (strong) {
      vcode_reg_t severity_reg = psl_assert_severity();
      vcode_reg_t false_reg = emit_const(vtype_bool(), 0);
      vcode_reg_t locus = psl_debug_locus(fsm->src);
      emit_assert(false_reg, VCODE_INVALID_REG, VCODE_INVALID_REG,
                  severity_reg, locus, VCODE_INVALID_REG,
                  VCODE_INVALID_REG);
      emit_return(VCODE_INVALID_REG);
   }
   else
      emit_unreachable(VCODE_INVALID_REG);

   vcode_select_block(prev_bb);

   const bool has_prev = vcode_count_ops() > 0;

   emit_return(emit_const(vint32, fsm->next_id + 1));

   vcode_select_block(case_bb);

   if (has_prev)
      emit_enter_state(emit_const(vint32, fsm->next_id), VCODE_INVALID_REG);

   emit_case(state_reg, abort_bb, state_ids, state_bb, fsm->next_id + 1);

   unit_registry_finalise(ur, lu);

   psl_fsm_free(fsm);
}

static void psl_lower_clock_decl(unit_registry_t *ur, lower_unit_t *parent,
                                 psl_node_t p, ident_t label)
{
   vcode_state_t state;
   vcode_state_save(&state);

   vcode_unit_t context = get_vcode(parent);
   vcode_select_unit(context);

   ident_t prefix = vcode_unit_name(context);
   ident_t name = ident_prefix(prefix, label, '.');

   vcode_unit_t vu = emit_function(name, psl_to_object(p), context);
   vcode_set_result(vtype_bool());

   vcode_type_t vcontext = vtype_context(prefix);
   emit_param(vcontext, vcontext, ident_new("context"));

   lower_unit_t *lu = lower_unit_new(ur, parent, vu, NULL, NULL);
   unit_registry_put(ur, lu);

   vcode_reg_t clk_reg = lower_rvalue(lu, psl_tree(p));
   emit_return(clk_reg);

   unit_registry_finalise(ur, lu);

   vcode_state_restore(&state);

   vcode_type_t vtrigger = vtype_trigger();
   vcode_var_t var = emit_var(vtrigger, vtrigger, label, 0);

   vcode_reg_t context_reg = emit_context_upref(0);
   vcode_reg_t args[] = { context_reg };
   vcode_reg_t trigger_reg = emit_function_trigger(name, args, ARRAY_LEN(args));
   emit_store(trigger_reg, var);

   lower_put_vcode_obj(p, var, parent);
}

void psl_lower_decl(unit_registry_t *ur, lower_unit_t *parent, psl_node_t p,
                    ident_t label)
{
   switch (psl_kind(p)) {
   case P_CLOCK_DECL:
      psl_lower_clock_decl(ur, parent, p, label);
      break;
   default:
      fatal_at(psl_loc(p), "cannot lower PSL declaration kind %s",
               psl_kind_str(psl_kind(p)));
   }
}
