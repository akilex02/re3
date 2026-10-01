#ifndef MC_BRIDGE_H
#define MC_BRIDGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

const char *mc_bridge_version(void);

/* Survival core. Item ids are indices of the C++ item table (0 = air / empty). Items the
 * table does not know never come back: such drops are discarded (drops_lost) and such
 * crafting outputs are hidden. Every call returns its failure value on a null or invalid
 * argument instead of aborting. */
typedef struct McSurvival McSurvival;
typedef struct { int32_t item; int32_t count; int32_t damage; int32_t max_damage; } McStack; /* item 0 = vacío */
typedef struct {
	int32_t broken;
	int32_t block_id;
	float progress;
	int32_t drops_added;	/* items put in the inventory */
	int32_t tool_broke;
	int32_t drops_lost;	/* items dropped but unknown to the item table or with no room */
} McMineResult;
typedef int32_t (*McGetBlockFn)(void *ctx, int32_t x, int32_t y, int32_t z);   /* celda GTA -> id */
typedef void (*McSetBlockFn)(void *ctx, int32_t x, int32_t y, int32_t z, int32_t id);

/* item_names[0] is air; the leading run of known blocks after it are the placeable blocks. NULL on failure. */
McSurvival *mc_survival_create(const char *client_jar, const char *item_catalog_json,
                               const char *const *item_names, int32_t item_count);
void mc_survival_destroy(McSurvival *s);
const char *mc_survival_last_error(void);          /* del último create fallido; nunca NULL; "" si no hubo */

/* area: 0 inventario (0..8 hotbar, 9..35 mochila, 36..39 armadura), 1 rejilla 2x2, 2 rejilla 3x3, 3 cursor, 4 salida 2x2, 5 salida 3x3 */
int32_t mc_inv_get(McSurvival *s, int32_t area, int32_t index, McStack *out);   /* 1 si hay item, 0 vacío/error (out a cero) */
int32_t mc_inv_click(McSurvival *s, int32_t area, int32_t index, int32_t right, int32_t shift);  /* 1 ok; solo áreas 0..2 */
int32_t mc_inv_take_output(McSurvival *s, int32_t workbench, int32_t shift);    /* 1 si fabricó */
int32_t mc_inv_close(McSurvival *s);               /* devuelve rejilla y cursor al inventario; sobrantes que no caben: nº de stacks perdidos; -1 si error */
int32_t mc_inv_add(McSurvival *s, int32_t item, int32_t count);                 /* count 1..255; sobrante que no cupo; -1 si error */
int32_t mc_inv_consume(McSurvival *s, int32_t slot, int32_t count);             /* 1 si quitó */

int32_t mc_survival_mine(McSurvival *s, McGetBlockFn get, McSetBlockFn set, void *ctx,
                         const double eye[3], const double dir[3], int32_t attacking, int32_t on_ground,
                         int32_t selected_slot, McMineResult *out);              /* un tick de 1/20 s; 1 ok, 0 error (slot fuera de 0..8, get/set NULL) */
void mc_survival_stop_mining(McSurvival *s);

int32_t mc_survival_save(McSurvival *s, const char *path);                       /* 1 ok */
int32_t mc_survival_load(McSurvival *s, const char *path);                       /* 1 ok; 0 si falta o está corrupto (queda vacío) */

#ifdef __cplusplus
}
#endif
#endif
