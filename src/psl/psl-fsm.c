//
//  Copyright (C) 2023-2024 Nick Gasson
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
#include "array.h"
#include "common.h"
#include "diag.h"
#include "ident.h"
#include "mask.h"
#include "psl/psl-fsm.h"
#include "psl/psl-node.h"
#include "psl/psl-phase.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#define CANNOT_HANDLE(p) do {                                   \
      fatal_at(psl_loc(p), "cannot handle PSL kind %s in %s",   \
               psl_kind_str(psl_kind(p)),  __FUNCTION__);       \
   } while (0)

static fsm_state_t *build_node(psl_fsm_t *fsm, fsm_state_t *state,
                               psl_node_t p);

static fsm_state_t *add_state(psl_fsm_t *fsm, psl_node_t where)
{
   fsm_state_t *s = xcalloc(sizeof(fsm_state_t));
   s->id    = fsm->next_id++;
   s->where = where;

   *(fsm->tail) = s;
   fsm->tail = &(s->next);

   return s;
}

static void insert_edge(fsm_state_t *from, fsm_state_t *to, edge_kind_t kind,
                        psl_node_t guard, fsm_edge_t **where)
{
   fsm_edge_t *e = xcalloc(sizeof(fsm_edge_t));
   e->next  = *where;
   e->kind  = kind;
   e->dest  = to;
   e->guard = guard;

   *where = e;
}

static void add_edge(fsm_state_t *from, fsm_state_t *to, edge_kind_t kind,
                     psl_node_t guard)
{
   fsm_edge_t **p = &(from->edges);
   for (; *p && (guard == NULL || (*p)->guard != NULL); p = &((*p)->next));

   insert_edge(from, to, kind, guard, p);
}

static int64_t get_number(tree_t t)
{
   int64_t result = 0;
   if (!folded_int(t, &result))
      error_at(tree_loc(t), "static value of PSL Number is not known");
   else if (result < 0)
      warn_at(tree_loc(t), "PSL Number %"PRIi64" is negative", result);

   return result;
}

static void connect_abort(fsm_state_t *from, fsm_state_t *to, psl_node_t guard,
                          bit_mask_t *visited)
{
   if (from->edges == NULL)
      return;   // Final state
   else if (mask_test_and_set(visited, from->id))
      return;   // Cycle

   for (fsm_edge_t *e = from->edges; e; e = e->next)
      connect_abort(e->dest, to, guard, visited);

   insert_edge(from, to, EDGE_EPSILON, guard, &(from->edges));
}

static void connect_default(fsm_state_t *from, fsm_state_t *to,
                            bit_mask_t *visited)
{
   if (from->edges == NULL)
      return;   // Final state
   else if (mask_test_and_set(visited, from->id))
      return;   // Cycle

   bool have_def = false;
   for (fsm_edge_t *e = from->edges; e; e = e->next) {
      connect_default(e->dest, to, visited);
      have_def |= e->guard == NULL;
   }

   if (!have_def)
      add_edge(from, to, EDGE_NEXT, NULL);
}

static fsm_state_t *build_logical(psl_fsm_t *fsm, fsm_state_t *state,
                                  psl_node_t p)
{
   psl_node_t lhs = psl_operand(p, 0);
   psl_node_t rhs = psl_operand(p, 1);

   switch (psl_subkind(p)) {
   case PSL_LOGIC_IFF:
      {
         // Only legal with Boolean HDL expression
         fsm_state_t *left = add_state(fsm, p);
         fsm_state_t *right = add_state(fsm, p);
         fsm_state_t *accept = add_state(fsm, p);
         add_edge(state, left, EDGE_EPSILON, lhs);
         add_edge(state, right, EDGE_EPSILON, rhs);
         add_edge(left, accept, EDGE_EPSILON, rhs);
         add_edge(right, accept, EDGE_EPSILON, lhs);
         add_edge(state, accept, EDGE_EPSILON, NULL);
         return accept;
      }

   case PSL_LOGIC_IF:
      {
         fsm_state_t *left = add_state(fsm, p);
         fsm_state_t *right = build_node(fsm, left, rhs);
         add_edge(state, left, EDGE_EPSILON, lhs);
         add_edge(state, right, EDGE_EPSILON, NULL);
         return right;
      }

   case PSL_LOGIC_OR:
      {
         fsm_state_t *accept = add_state(fsm, p), *final;

         // At least one operand must be Boolean
         if (psl_kind(lhs) == P_HDL_EXPR) {
            add_edge(state, accept, EDGE_EPSILON, lhs);
            final = build_node(fsm, state, rhs);
         }
         else {
            add_edge(state, accept, EDGE_EPSILON, rhs);
            final = build_node(fsm, state, lhs);
         }

         add_edge(final, accept, EDGE_EPSILON, NULL);
         return accept;
      }

   default:
      CANNOT_HANDLE(p);
   }
}

static fsm_state_t *build_until(psl_fsm_t *fsm, fsm_state_t *state,
                                psl_node_t p)
{
   psl_node_t lhs = psl_operand(p, 0);
   psl_node_t rhs = psl_operand(p, 1);

   if (psl_flags(p) & PSL_F_INCLUSIVE) {
      fsm_state_t *test = add_state(fsm, p);
      add_edge(state, test, EDGE_EPSILON, lhs);

      fsm_state_t *new = add_state(fsm, p);
      add_edge(test, new, EDGE_NEXT, rhs);
      add_edge(test, state, EDGE_NEXT, NULL);

      return new;
   }
   else {
      fsm_state_t *new = add_state(fsm, p);
      add_edge(state, new, EDGE_NEXT, rhs);
      add_edge(state, state, EDGE_NEXT, lhs);

      return new;
   }
}

static fsm_state_t *build_abort(psl_fsm_t *fsm, fsm_state_t *state,
                                psl_node_t p)
{
   psl_node_t lhs = psl_operand(p, 0);
   psl_node_t rhs = psl_operand(p, 1);

   fsm_state_t *final = build_node(fsm, state, lhs);

   fsm_state_t *sink = add_state(fsm, p);
   sink->accept = true;

   LOCAL_BIT_MASK visited;
   mask_init(&visited, fsm->next_id);

   connect_abort(state, sink, rhs, &visited);

   return final;
}

static fsm_state_t *build_sere(psl_fsm_t *fsm, fsm_state_t *state, psl_node_t p)
{
   const int nops = psl_operands(p);

   for (int i = 0; i < nops; i++) {
      psl_node_t rhs = psl_operand(p, i);
      edge_kind_t ekind = EDGE_NEXT;

      switch (psl_subkind(p)) {
      case PSL_SERE_FUSION:
         ekind = EDGE_EPSILON;
      case PSL_SERE_CONCAT:
         if (i + 1 < nops) {
            fsm_state_t *lhs = build_node(fsm, state, rhs);
            if (lhs != state) {
               state = add_state(fsm, p);
               add_edge(lhs, state, ekind, NULL);
            }
         }
         else
            state = build_node(fsm, state, rhs);
         break;
      default:
         CANNOT_HANDLE(p);
      }
   }

   return state;
}

static void get_repeat_bounds(psl_node_t p, int *low, int *high, bool *infinite,
                              bool *noncon, bool *goto_rep)
{
   psl_node_t r = psl_repeat(p);
   psl_repeat_t kind = psl_subkind(r);

   *low = 0;
   *high = 0;
   *noncon = false;
   *goto_rep = false;
   *infinite = false;

   switch (kind) {
   case PSL_PLUS_REPEAT:
      *low = 1;
      *high = INT32_MAX;
      break;

   case PSL_GOTO_REPEAT:
      *goto_rep = true;
      // fallthrough

   case PSL_EQUAL_REPEAT:
      *noncon = true;
      // fallthrough

   case PSL_TIMES_REPEAT:
      if (psl_has_tree(r)) {
         tree_t t_r = psl_tree(r);
         if (tree_kind(t_r) == T_RANGE) {
            *low = get_number(tree_left(t_r));
            *high = get_number(tree_right(t_r));
         }
         else {
            *low = get_number(t_r);
            *high = *low;
         }
      }
      else {
         *low = 0;
         *high = INT32_MAX;
      }
      break;

   default:
      CANNOT_HANDLE(r);
   }

   if (*high == INT32_MAX) {
      *infinite = true;
      *high = *low;
   }
}

static fsm_state_t *build_repeated_sere(psl_fsm_t *fsm, fsm_state_t *state,
                                        psl_node_t p)
{
   assert(psl_has_repeat(p));

   int low, high;
   bool infinite, noncon, goto_rep;

   get_repeat_bounds(p, &low, &high, &infinite, &noncon, &goto_rep);

   fsm_state_t *skip = (high > low) ? add_state(fsm, p) : NULL;
   fsm_state_t *initial = state;
   fsm_state_t *last_but_one = NULL;

   for (int i = 0; i < high; i++) {
      bool is_last = (i == high - 1) ? true : false;

      last_but_one = state;
      state = build_sere(fsm, state, p);

      if (noncon) {
         fsm_state_t *wait = add_state(fsm, p);
         add_edge(last_but_one, wait, EDGE_EPSILON, NULL);
         add_edge(wait, last_but_one, EDGE_NEXT, NULL);
      }

      if (!is_last) {
         if (i >= low - 1)
            add_edge(state, skip, EDGE_EPSILON, NULL);

         fsm_state_t *curr = state;
         state = add_state(fsm, p);
         add_edge(curr, state, EDGE_NEXT, NULL);
      }
      else if (noncon && !goto_rep) {
         fsm_state_t *aux = add_state(fsm, p);
         fsm_state_t *dead = add_state(fsm, p);
         fsm_state_t *wait = add_state(fsm, p);
         add_edge(state, aux, EDGE_NEXT, NULL);
         add_edge(aux, dead, EDGE_EPSILON, psl_operand(p, 0));
         add_edge(aux, wait, EDGE_EPSILON, NULL);
         add_edge(wait, aux, EDGE_NEXT, NULL);
         add_edge(wait, state, EDGE_EPSILON, NULL);
      }
   }

   if (skip)
      add_edge(skip, state, EDGE_EPSILON, NULL);

   if (initial != state) {
      if (low == 0)
         add_edge(initial, state, EDGE_EPSILON, NULL);
      if (infinite)
         add_edge(state, last_but_one, EDGE_NEXT, NULL);
   }

   return state;
}

static fsm_state_t *build_sequence(psl_fsm_t *fsm, fsm_state_t *state, psl_node_t p)
{
   if (psl_has_repeat(p))
      return build_repeated_sere(fsm, state, p);

   return build_sere(fsm, state, p);
}

static fsm_state_t *build_next(psl_fsm_t *fsm, fsm_state_t *state, psl_node_t p)
{
   if (psl_has_delay(p)) {
      const int cycles = get_number(psl_delay(p));
      for (int i = 0; i < cycles; i++) {
         fsm_state_t *new = add_state(fsm, p);
         add_edge(state, new, EDGE_NEXT, NULL);
         state = new;
      }
   }
   else {
      fsm_state_t *new = add_state(fsm, p);
      add_edge(state, new, EDGE_NEXT, NULL);
      state = new;
   }

   return build_node(fsm, state, psl_value(p));
}

static fsm_state_t *build_eventually(psl_fsm_t *fsm, fsm_state_t *state,
                                     psl_node_t p)
{
   fsm_state_t *wait = add_state(fsm, p);
   fsm_state_t *accept = build_node(fsm, wait, psl_value(p));

   LOCAL_BIT_MASK visited;
   mask_init(&visited, fsm->next_id);

   connect_default(wait, wait, &visited);

   add_edge(state, wait, EDGE_NEXT, NULL);
   wait->strong = true;

   return accept;
}

static fsm_state_t *build_before(psl_fsm_t *fsm, fsm_state_t *state,
                                 psl_node_t p)
{
   fsm_state_t *accept = add_state(fsm, p);
   fsm_state_t *fail = add_state(fsm, p);

   state->strong = !!(psl_flags(p) & PSL_F_STRONG);

   if (psl_flags(p) & PSL_F_INCLUSIVE) {
      add_edge(state, accept, EDGE_EPSILON, psl_operand(p, 0));
      add_edge(state, fail, EDGE_EPSILON, psl_operand(p, 1));
   }
   else {
      add_edge(state, fail, EDGE_EPSILON, psl_operand(p, 1));
      add_edge(state, accept, EDGE_EPSILON, psl_operand(p, 0));
   }

   add_edge(state, state, EDGE_NEXT, NULL);

   return accept;
}

static fsm_state_t *build_suffix_impl(psl_fsm_t *fsm, fsm_state_t *state,
                                      psl_node_t p)
{
   fsm_state_t *left = build_node(fsm, state, psl_operand(p, 0));
   fsm_state_t *right = add_state(fsm, p);
   fsm_state_t *vacuous = add_state(fsm, p);

   if (psl_subkind(p) == PSL_SUFFIX_OVERLAP)
      add_edge(left, right, EDGE_EPSILON, NULL);
   else
      add_edge(left, right, EDGE_NEXT, NULL);

   LOCAL_BIT_MASK visited;
   mask_init(&visited, fsm->next_id);

   connect_default(state, vacuous, &visited);

   fsm_state_t *final = build_node(fsm, right, psl_operand(p, 1));
   add_edge(vacuous, final, EDGE_EPSILON, NULL);

   return final;
}

static fsm_state_t *build_node(psl_fsm_t *fsm, fsm_state_t *state, psl_node_t p)
{
   switch (psl_kind(p)) {
   case P_NEVER:
      fsm->kind = FSM_NEVER;
      return build_node(fsm, state, psl_value(p));
   case P_ALWAYS:
      fsm->kind = FSM_ALWAYS;
      return build_node(fsm, state, psl_value(p));
   case P_HDL_EXPR:
      {
         fsm_state_t *new = add_state(fsm, p);
         add_edge(state, new, EDGE_EPSILON, p);
         return new;
      }
   case P_NEXT:
      return build_next(fsm, state, p);
   case P_SERE:
      return build_sequence(fsm, state, p);
   case P_LOGICAL:
      return build_logical(fsm, state, p);
   case P_UNTIL:
      return build_until(fsm, state, p);
   case P_EVENTUALLY:
      return build_eventually(fsm, state, p);
   case P_ABORT:
      return build_abort(fsm, state, p);
   case P_BEFORE:
      return build_before(fsm, state, p);
   case P_SUFFIX_IMPL:
      return build_suffix_impl(fsm, state, p);
   default:
      CANNOT_HANDLE(p);
   }
}

#ifdef DEBUG
static void psl_loops_dfs(psl_fsm_t *fsm, fsm_state_t *state,
                          bit_mask_t *discovered, bit_mask_t *finished)
{
   mask_set(discovered, state->id);

   for (fsm_edge_t *e = state->edges; e; e = e->next) {
      if (e->kind != EDGE_EPSILON || mask_test(finished, e->dest->id))
         continue;
      else if (mask_test(discovered, e->dest->id)) {
         psl_fsm_dump(fsm, "loop.dot");
         fatal_trace("detected loop in PSL state machine %d -> %d",
                     state->id, e->dest->id);
      }
      else
         psl_loops_dfs(fsm, e->dest, discovered, finished);
   }

   mask_clear(discovered, state->id);
   mask_set(finished, state->id);
}

static void psl_detect_loops(psl_fsm_t *fsm)
{
   LOCAL_BIT_MASK discovered, finished;
   mask_init(&discovered, fsm->next_id);
   mask_init(&finished, fsm->next_id);

   for (fsm_state_t *it = fsm->states; it; it = it->next) {
      if (!mask_test(&finished, it->id))
         psl_loops_dfs(fsm, it, &discovered, &finished);
   }

   assert(mask_popcount(&discovered) == 0);
   assert(mask_popcount(&finished) == fsm->next_id);
}
#endif

psl_fsm_t *psl_fsm_new(psl_node_t p)
{
   psl_fsm_t *fsm = xcalloc(sizeof(psl_fsm_t));
   fsm->tail = &(fsm->states);
   fsm->src  = p;
   fsm->kind = psl_kind(p) == P_COVER ? FSM_COVER : FSM_BARE;

   fsm_state_t *initial = add_state(fsm, p), *final = initial;
   initial->initial = true;

   final = build_node(fsm, initial, psl_value(p));
   final->accept = true;

   DEBUG_ONLY(psl_detect_loops(fsm));
   return fsm;
}

void psl_fsm_free(psl_fsm_t *fsm)
{
   free(fsm);
}

static void psl_dump_label(FILE *f, psl_node_t p)
{
   LOCAL_TEXT_BUF tb = tb_new();
   capture_syntax(tb);

   psl_dump(p);

   for (const char *p = tb_get(tb); *p; p++) {
      if (*p == '"')
         fputs("\\\"", f);
      else
         fputc(*p, f);
   }

   capture_syntax(NULL);
}

void psl_fsm_dump(psl_fsm_t *fsm, const char *fname)
{
   FILE *f = fopen(fname, "w");
   if (f == NULL)
      fatal_errno("%s", fname);

   fprintf(f, "digraph psl {\n");

   for (fsm_state_t *s = fsm->states; s; s = s->next) {
      if (s->accept)
         fprintf(f, "%d [peripheries=2];\n", s->id);

      for (fsm_edge_t *e = s->edges; e; e = e->next) {
         fprintf(f, "%d -> %d [", s->id, e->dest->id);
         if (e->guard != NULL) {
            fprintf(f, "label=\"");
            psl_dump_label(f, e->guard);
            fputs("\",", f);
         }
         if (e->kind == EDGE_EPSILON)
            fputs("style=dashed,", f);
         fprintf(f, "];\n");
      }
   }

   fprintf(f, "}\n");
   fclose(f);

   const char *args[] = {
      "/usr/bin/dot",
      "-Tsvg",
      "-O",
      fname,
      NULL
   };
   run_program(args);

   debugf("wrote PSL state machine graph to %s.svg", fname);
}

bool psl_fsm_repeating(psl_fsm_t *fsm)
{
   return fsm->kind == FSM_COVER || fsm->kind == FSM_ALWAYS
      || fsm->kind == FSM_NEVER;
}
