#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include "dataflow.h"
#include "error.h"
#include "list.h"
#include "set.h"

typedef struct vertex_t	vertex_t;
typedef struct task_t	task_t;

/* cfg_t: a control flow graph. */
struct cfg_t {
	size_t			n;		/* number of vertices		*/
	size_t			s;		/* width of bitvectors		*/
	size_t			max_succ;	/* max number of successors	*/
	vertex_t*		vertex;		/* array of vertex		*/
};

/* vertex_t: a control flow graph vertex. */
struct vertex_t {
	size_t			index;		/* can be used for debugging	*/
	set_t*			set[NSETS];	/* live in from this vertex	*/
	set_t*			prev;		/* alternating with set[IN]	*/
	size_t			nsucc;		/* number of successor vertices */
	vertex_t**		succ;		/* successor vertices 		*/
	list_t*			pred;		/* predecessor vertices		*/
	bool			listed;		/* on worklist			*/
	int			dfnum;		/* depth first search number	*/
};

static void clean_vertex(vertex_t* v);
static void init_vertex(vertex_t* v, size_t index, size_t nsymbol, size_t max_succ);

cfg_t* sequential_new_cfg(size_t nvertex, size_t nsymbol, size_t max_succ)
{
	size_t		i;
	cfg_t*		cfg;

	cfg = calloc(1, sizeof(cfg_t));
	if (cfg == NULL)
		error("out of memory");

	cfg->n = nvertex;
	cfg->s = nsymbol;
	cfg->max_succ = max_succ;

	cfg->vertex = calloc(nvertex, sizeof(vertex_t));
	if (cfg->vertex == NULL)
		error("out of memory");

	for (i = 0; i < nvertex; i += 1)
		init_vertex(&cfg->vertex[i], i, nsymbol, max_succ);

	return cfg;
}

static void clean_vertex(vertex_t* v)
{
	int		i;

	for (i = 0; i < NSETS; i += 1)
		free_set(v->set[i]);
	free_set(v->prev);
	free(v->succ);
	free_list(&v->pred);
}

static void init_vertex(vertex_t* v, size_t index, size_t nsymbol, size_t max_succ)
{
	int		i;

	v->index	= index;
	v->succ		= calloc(max_succ, sizeof(vertex_t*));

	if (v->succ == NULL)
		error("out of memory");
	
	for (i = 0; i < NSETS; i += 1)
		v->set[i] = new_set(nsymbol);

	v->prev = new_set(nsymbol);
}

void sequential_free_cfg(cfg_t* cfg)
{
	size_t		i;

	for (i = 0; i < cfg->n; i += 1)
		clean_vertex(&cfg->vertex[i]);
	free(cfg->vertex);
	free(cfg);
}

void sequential_connect(cfg_t* cfg, size_t pred, size_t succ)
{
	vertex_t*	u;
	vertex_t*	v;

	u = &cfg->vertex[pred];
	v = &cfg->vertex[succ];

	u->succ[u->nsucc++ ] = v;
	insert_last(&v->pred, u);
}

bool sequential_testbit(cfg_t* cfg, size_t v, set_type_t type, size_t index)
{
	return test(cfg->vertex[v].set[type], index);
}

void sequential_setbit(cfg_t* cfg, size_t v, set_type_t type, size_t index)
{
	set(cfg->vertex[v].set[type], index);
}

void sequential_liveness(cfg_t* cfg)
{
	vertex_t*	u;
	vertex_t*	v;
	set_t*		prev;
	size_t		i;
	size_t		j;
	list_t*		worklist;
	list_t*		p;
	list_t*		h;

	worklist	= NULL;

	for (i = 0; i < cfg->n; ++i) {
		u = &cfg->vertex[i];

		insert_last(&worklist, u);
		u->listed = true;
	}

	while ((u = remove_first(&worklist)) != NULL) {
		u->listed = false;

		reset(u->set[OUT]);

		for (j = 0; j < u->nsucc; ++j)
			or(u->set[OUT], u->set[OUT], u->succ[j]->set[IN]);

		prev = u->prev;
		u->prev = u->set[IN];
		u->set[IN] = prev;

		/* in our case liveness information... */
		propagate(u->set[IN], u->set[OUT], u->set[DEF], u->set[USE]);

		if (u->pred != NULL && !equal(u->prev, u->set[IN])) {
			p = h = u->pred;
			do {
				v = p->data;

				if (!v->listed) {
					v->listed = true;
					insert_last(&worklist, v);
				}

				p = p->succ;

			} while (p != h);
		}
	}
}

void sequential_print_sets(cfg_t* cfg, FILE *fp)
{
        size_t          i;
        vertex_t*       u;

        for (i = 0; i < cfg->n; ++i) {
                u = &cfg->vertex[i];
                fprintf(fp, "use[%zu] = ", u->index);
                print_set(u->set[USE], fp);
                fprintf(fp, "def[%zu] = ", u->index);
                print_set(u->set[DEF], fp);
                fputc('\n', fp);
                fprintf(fp, "in[%zu] = ", u->index);
                print_set(u->set[IN], fp);
                fprintf(fp, "out[%zu] = ", u->index);
                print_set(u->set[OUT], fp);
                fputc('\n', fp);
        }
}

static int		dfnum;

static bool visited(vertex_t* u)
{
	return u->dfnum >= 1;
}

static void dfs(vertex_t* v)
{
	size_t		k;

	if (visited(v))
		return;

	v->dfnum 	= ++dfnum;

	for (k = 0; k < v->nsucc; k += 1)
		dfs(v->succ[k]);
}

extern void new_edge(size_t, size_t);

void check_cfg(cfg_t* cfg)
{
	size_t		i;
	size_t		j;

	for (dfnum = i = 0; i < cfg->n; i += 1)
		cfg->vertex[i].dfnum = 0;

	dfs(&cfg->vertex[0]);

	for (i = 0; i < cfg->n; i += 1) {
		if (!visited(&cfg->vertex[i])) {
			for (j = i-1; j > 0; j -= 1) {
				if (cfg->vertex[j].nsucc < cfg->max_succ) {
					sequential_connect(cfg, j, i);
					new_edge(j, i);
					break;
				}
			}
			if (j == 0)
				error("found no vertex");
		}
	}

	for (dfnum = i = 0; i < cfg->n; i += 1)
		cfg->vertex[i].dfnum = 0;

	dfs(&cfg->vertex[0]);

	for (i = 0; i < cfg->n; i += 1)
		assert(cfg->vertex[i].dfnum > 0);
}
