#include "gdk/gdk.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "gtk/gtkcssprovider.h"
#include "gtk/gtkdropdown.h"
#include "gtk/gtkshortcut.h"
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

typedef struct node{
  const char *barcode;
  const char *name;
  const char *price;
  const char *type;
  const char *brand;
  int quantity;
  GtkWidget *scan_label;
  GtkWidget *scan_dropdown;
  const char **quantity_dropdown;
  GtkWidget *scan_remove;
  struct node *next;
} node;

node *scanner_list = NULL;
GtkWidget *scanner_grid;
GtkWidget *price_total;
char *price_total_default = "Total: $";

GtkWidget *inventory_grid;
GtkWidget *inventory_box;

sqlite3 *db;
const char *errMsg = 0;
sqlite3_stmt *stmt;
int rc; 

const char *numbers[101];

static void update_inventory(GtkWidget* widget, char **data){
  char *query_template = "UPDATE products set";
  size_t query_size = snprintf(NULL, 0, "%s %s = %s WHERE barcode = %s", query_template, data[1], gtk_editable_get_text(GTK_EDITABLE(widget)), data[0]);
  char *query = malloc((sizeof(char) * query_size) + 1);
  sprintf(query, "%s %s = %s WHERE barcode = %s", query_template, data[1], gtk_editable_get_text(GTK_EDITABLE(widget)), data[0]);
  g_print("soy godddd: %s\n", query);
  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL); 

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db)); 
  }
  else {
    printf("Update succesfully\n");
  }
}

static void sql_inventory_product(GtkWidget *inventory_brand, GtkWidget *inventory_products_box){

  GtkWidget *inventory_product_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  GtkWidget *inventory_product_box_frame = gtk_frame_new(NULL);
  gtk_expander_set_child(GTK_EXPANDER(inventory_brand), inventory_products_box);
  gtk_box_append(GTK_BOX(inventory_products_box), inventory_product_box_frame);
  gtk_frame_set_child(GTK_FRAME(inventory_product_box_frame), inventory_product_box);
  gtk_widget_set_margin_start(inventory_product_box_frame, 15);
  gtk_widget_set_margin_start(inventory_product_box, 4);
  gtk_widget_set_margin_end(inventory_product_box, 4);

  const char *barcode_sql = (const char*)sqlite3_column_text(stmt, 3);
  GtkWidget *barcode_label = gtk_label_new(barcode_sql);
  gtk_box_append(GTK_BOX(inventory_product_box), barcode_label);

  const char *name_sql = (const char*)sqlite3_column_text(stmt, 2);
  GtkWidget *name_label = gtk_label_new(name_sql);
  gtk_box_append(GTK_BOX(inventory_product_box), name_label);
  gtk_widget_set_halign(name_label, GTK_ALIGN_CENTER);

  const char *price_sql = (const char*)sqlite3_column_text(stmt, 5);
  GtkWidget *price_label = gtk_editable_label_new(price_sql);
  gtk_box_append(GTK_BOX(inventory_product_box), price_label);

  char **price_data = malloc(sizeof(char *) * 2);
  price_data[0] = malloc(( sizeof(char) * strlen(barcode_sql) ) + 1);
  strcpy(price_data[0], barcode_sql);
  price_data[1] = malloc(( sizeof(char) * strlen("price") ) + 1);
  strcpy(price_data[1], "price");

  g_signal_connect(price_label, "changed", G_CALLBACK(update_inventory), price_data);

  const char *quantity_sql = (const char*)sqlite3_column_text(stmt, 4);
  GtkWidget *quantity_label = gtk_editable_label_new(quantity_sql);
  gtk_box_append(GTK_BOX(inventory_product_box), quantity_label);

  char **quantity_data = malloc(sizeof(char *) * 2);
  quantity_data[0] = malloc(( sizeof(char) * strlen(barcode_sql) ) + 1);
  strcpy(quantity_data[0], barcode_sql);
  quantity_data[1] = malloc(( sizeof(char) * strlen("quantity") ) + 1);
  strcpy(quantity_data[1], "quantity");

  g_signal_connect(quantity_label, "changed", G_CALLBACK(update_inventory), quantity_data);
}

static void sql_inventory(){
  char *query = "SELECT type, brand, name, barcode, quantity, price FROM products ORDER BY type;";

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
  }

  int i = 0;

  const char *type_sql = NULL;
  char *type_sql_temp = NULL;

  const char *brand_sql = NULL;
  char *brand_sql_temp = NULL;

  GtkWidget *inventory_brand;
  GtkWidget *inventory_brand_box;
  GtkWidget *inventory_brand_list_box;

  GtkWidget *inventory_products_box; 
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    i++;
    g_print("numberrrr: %d\n", i);
    int col_count = sqlite3_column_count(stmt);
    
    char *pango_markup = "<span size=\"20pt\">";
    char *pango_markup2 = "<span size=\"15pt\">";
    char *pango_markup_end = "</span>";
    size_t len_pango_markup = strlen(pango_markup);
    size_t len_pango_markup2 = strlen(pango_markup2);
    size_t len_pango_markup_end = strlen(pango_markup_end);

    if (type_sql != NULL && type_sql_temp != NULL && strcmp(type_sql, type_sql_temp) == 0) {
      if (strcmp(brand_sql, brand_sql_temp) == 0) {
        sql_inventory_product(inventory_brand, inventory_products_box);
        continue; 
      }
      size_t len_brand = strlen(brand_sql);
      char *brand = (char *)malloc(( len_brand * sizeof(char) ) + (len_pango_markup2 * sizeof(char)) + (len_pango_markup_end * sizeof(char)) + 1);
      sprintf(brand, "%s%s%s", pango_markup2, brand_sql, pango_markup_end);
      inventory_brand = gtk_expander_new_with_mnemonic(brand);
      gtk_expander_set_use_markup(GTK_EXPANDER(inventory_brand), TRUE);
      gtk_box_append(GTK_BOX(inventory_brand_box), inventory_brand);
      g_print("brand_sql_temp: %s\n", brand_sql_temp);
      g_print("brand_sql: %s\n", brand_sql);

      free(brand_sql_temp);
      brand_sql_temp = malloc(( strlen(brand_sql) * sizeof(char) ) + 1);
      strcpy(brand_sql_temp, brand_sql);

      inventory_products_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
      sql_inventory_product(inventory_brand, inventory_products_box);
      continue;
    }

    inventory_brand_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);

    if (brand_sql_temp != NULL) {
      free(brand_sql_temp);
      free(type_sql_temp);
    }

    type_sql = (const char*)sqlite3_column_text(stmt, 0);
    type_sql_temp = malloc(( strlen(type_sql) * sizeof(char) ) + 1);
    strcpy(type_sql_temp, type_sql);
    size_t len_type = strlen(type_sql);
    char *type = (char *)malloc(( len_type * sizeof(char) ) + (len_pango_markup * sizeof(char)) + (len_pango_markup_end * sizeof(char)) + 1);
    sprintf(type, "%s%s%s", pango_markup, type_sql, pango_markup_end);
    GtkWidget *inventory_type = gtk_expander_new_with_mnemonic(type);
    gtk_expander_set_use_markup(GTK_EXPANDER(inventory_type), TRUE);

    brand_sql = (const char*)sqlite3_column_text(stmt, 1);
    brand_sql_temp = malloc(( strlen(brand_sql) * sizeof(char) ) + 1);
    strcpy(brand_sql_temp, brand_sql);
    size_t len_brand = strlen(brand_sql);
    char *brand = (char *)malloc(( len_brand * sizeof(char) ) + (len_pango_markup2 * sizeof(char)) + (len_pango_markup_end * sizeof(char)) + 1);
    sprintf(brand, "%s%s%s", pango_markup2, brand_sql, pango_markup_end);
    inventory_brand = gtk_expander_new_with_mnemonic(brand);
    gtk_expander_set_use_markup(GTK_EXPANDER(inventory_brand), TRUE);

    gtk_box_append(GTK_BOX(inventory_box), inventory_type);
    gtk_expander_set_child(GTK_EXPANDER(inventory_type), inventory_brand_box);
    gtk_box_append(GTK_BOX(inventory_brand_box), inventory_brand);

    inventory_products_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
    sql_inventory_product(inventory_brand, inventory_products_box);
    
    free(type);
    free(brand);
  }

  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Error fetching rows: %s\n", sqlite3_errmsg(db));
  }
}

static char* sql_query_example(){
  char *query = "SELECT * FROM products;";

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  char *result; 
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const char *col_name = sqlite3_column_name(stmt, 1);
    const char *col_value = (const char*)sqlite3_column_text(stmt, 1);
    printf("%s = %s\n", col_name, col_value);
  }

  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Error fetching rows: %s\n", sqlite3_errmsg(db));
  }
  return result;
}

static char* sql_query(const char *barcode, char *sql){
  char *query_template = "SELECT "; 
  char *query_template2 = " FROM products WHERE barcode = ";
  int query_len = snprintf(NULL, 0, "%s%s%s%s;", query_template, sql, query_template2, barcode);
  char *query = malloc(query_len * sizeof(char) + 1);
  sprintf(query, "%s%s%s%s;", query_template, sql, query_template2, barcode);

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  char *result; 
  g_print("fuck you\n");
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    // Get column count
    int col_count = sqlite3_column_count(stmt);
    // Print each column
    const char *col_name = sqlite3_column_name(stmt, 0);
    const char *col_value = (const char*)sqlite3_column_text(stmt, 0);
    size_t length = strlen(col_value); 
    result = (char *)malloc(( length * sizeof(char) ) + 1);
    strcpy(result, col_value);
  }
  free(query);

  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Error fetching rows: %s\n", sqlite3_errmsg(db));
  }
  return result;
}

static void total_price(){
  int total = 0;
  for (node *ptr = scanner_list; ptr != NULL; ptr = ptr->next) {
    GtkStringObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(ptr->scan_dropdown));
    const char* item_value = gtk_string_object_get_string(item);
    total += (atoi(ptr->price) * atoi(item_value));
  }
  int digits = snprintf(NULL, 0, "%d", total);
  if (digits < 0) {
    return; 
  }

  char *total_str = malloc(( digits + 1 ) + ( strlen(price_total_default) * sizeof(char)));
  sprintf(total_str, "%s%d", price_total_default, total);
  gtk_label_set_text(GTK_LABEL(price_total), total_str);
  free(total_str);
}

static void sql_sell(GtkWidget *widget, gpointer data){
  for (node *ptr = scanner_list; ptr != NULL; ptr = ptr->next) {
    GtkStringObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(ptr->scan_dropdown));
    const char* item_value = gtk_string_object_get_string(item);
    int quantity = ptr->quantity - atoi(item_value);
    g_print("quant: %d", quantity);
    int digits = snprintf(NULL, 0, "%d", quantity);
    if (digits < 0) {
      return; 
    }
    char *quantity_str = malloc(digits + 1);
    snprintf(quantity_str, digits + 1, "%d", quantity);

    char *query_template = "UPDATE products SET quantity =";
    char *query_template2 = "WHERE barcode =";
    const char* barcode = ptr->barcode;
    char *query = malloc(( strlen(query_template) * sizeof(char) ) + (strlen(query_template2) * sizeof(char)) + (strlen(quantity_str) * sizeof(char)) + ( strlen(barcode) * sizeof(char)));
    sprintf(query, "%s %s %s %s", query_template, quantity_str, query_template2, barcode); 
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL); 
    free(quantity_str);

    if (rc != SQLITE_OK) {
      fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
      return;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db)); 
    }
    else {
      printf("Update succesfully\n");
    }
    free(query);
    gtk_label_set_text(GTK_LABEL(price_total), "Total: ");
  }
}

static void print_dropdown(GtkWidget *widget, gpointer data){
  GtkStringObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(widget));
  const char* item_value = gtk_string_object_get_string(item);
}

static void free_scanner(GtkWidget *scanner_entry){
  for (node *ptr = scanner_list; ptr != NULL; ptr = scanner_list) {
    gtk_grid_remove(GTK_GRID(scanner_grid), ptr->scan_label); 
    gtk_grid_remove(GTK_GRID(scanner_grid), ptr->scan_dropdown); 
    gtk_grid_remove(GTK_GRID(scanner_grid), ptr->scan_remove); 
    free((void *)ptr->name);
    free((void *)ptr->brand);
    free((void *)ptr->type);
    free((void *)ptr->price);

    scanner_list = scanner_list->next; 
    free(ptr);
  }
  gtk_widget_grab_focus(scanner_entry);

  gtk_label_set_text(GTK_LABEL(price_total), "Total: ");
}

static void remove_scan(GtkWidget *widget){
  node *ptr = scanner_list;
  if (ptr->scan_remove == widget) {

    gtk_grid_remove(GTK_GRID(scanner_grid), ptr->scan_label); 
    gtk_grid_remove(GTK_GRID(scanner_grid), ptr->scan_dropdown); 
    gtk_grid_remove(GTK_GRID(scanner_grid), ptr->scan_remove); 
    free((void *)ptr->name);
    free((void *)ptr->brand);
    free((void *)ptr->type);
    free((void *)ptr->price);

    if (ptr->next == NULL) {
      scanner_list = NULL;
    }
    else {
      scanner_list = ptr->next;  
    }
    free(ptr);
    return;
  }

  while (ptr->next != NULL){
    if (ptr->next->scan_remove == widget) {
      gtk_grid_remove(GTK_GRID(scanner_grid), ptr->next->scan_label); 
      gtk_grid_remove(GTK_GRID(scanner_grid), ptr->next->scan_dropdown); 
      gtk_grid_remove(GTK_GRID(scanner_grid), ptr->next->scan_remove); 

      if (ptr->next->next == NULL) {
        gtk_widget_set_margin_top(ptr->scan_label, 15);
        gtk_widget_set_margin_top(ptr->scan_dropdown, 15);
        gtk_widget_set_margin_top(ptr->scan_remove, 15);
      }
      node *temp = ptr->next;
      ptr->next = ptr->next->next;
      free(temp);
      return;
    } 
    ptr = ptr->next;
  }
}

static void print_lol(){
  g_print("lolllll it workssss");
}

static void add_scan(GtkWidget *widget, gpointer data)
{
  GtkEntryBuffer *scanner_entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(widget));
  const char *scan = g_strdup(gtk_entry_buffer_get_text(scanner_entry_buffer));
  gtk_entry_buffer_set_text(scanner_entry_buffer, "", 0);

  char *sql_quantity = sql_query(scan, "quantity");
  g_print("%s",sql_quantity);
  if (sql_quantity == NULL) {
    return; 
  }
  int quantity = atoi(sql_quantity);

  if (scanner_list != NULL ) {   
    for (node *ptr = scanner_list; ptr != NULL; ptr = ptr->next) {
      if (strcmp(ptr->barcode, scan) == 0) {
        GtkStringObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(ptr->scan_dropdown));
        const char* item_value = gtk_string_object_get_string(item);

        if (atoi(item_value) == quantity) {
          return; 
        }
        guint n = atoi(item_value);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ptr->scan_dropdown), n);
        total_price();
        return;
      }
    }
  }

  const char *name = (const char *)sql_query(scan, "name");
  const char *brand = (const char *)sql_query(scan, "brand");
  const char *type = (const char *)sql_query(scan, "type");
  const char *price = (const char *)sql_query(scan, "price");


  size_t length_brand = strlen(brand); 
  size_t length_name = strlen(name); 
  size_t length_type = strlen(type); 
  size_t length_price = strlen(price); 

  char *label = malloc(( length_brand * sizeof(char) ) + (length_name * sizeof(char)) + (length_type * sizeof(char)) + (length_price * sizeof(char)) + 4);
  sprintf(label, "%s %s %s\t$%s", type, brand, name, price);

  const char *quantity_dropdown[quantity];
  for (int i = 0; i < quantity; i++) {
    int len = snprintf(NULL, 0, "%d", i);
    char *str = malloc(len * sizeof(char) + 1);
    sprintf(str, "%d", i + 1);
    quantity_dropdown[i] = str;
  }


  node *n = malloc(sizeof(node));
  n->barcode = scan;
  n->brand = brand;
  n->name = name;  
  n->type = type;
  n->scan_label = gtk_label_new(label);
  n->quantity_dropdown = quantity_dropdown;
  n->scan_dropdown = gtk_drop_down_new_from_strings(quantity_dropdown);
  n->scan_remove = gtk_button_new_with_label("Eliminar");
  n->quantity = quantity;
  n->price = price;
  n->next = scanner_list;
  scanner_list = n;

  free(label);

  if (n->next == NULL) {
    gtk_grid_attach(GTK_GRID(scanner_grid), n->scan_label, 1, 1, 1, 1);
    gtk_widget_set_margin_top(n->scan_label, 15);
    gtk_widget_set_margin_top(n->scan_dropdown, 15);
    gtk_widget_set_margin_top(n->scan_remove, 15);
  }
  else {
    gtk_grid_attach_next_to(GTK_GRID(scanner_grid), n->scan_label, n->next->scan_label, GTK_POS_BOTTOM, 1, 1);
  }

  gtk_widget_set_halign (n->scan_label, GTK_ALIGN_START);
  gtk_widget_set_valign (n->scan_label, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start(n->scan_label, 15);

  gtk_drop_down_set_selected(GTK_DROP_DOWN(n->scan_dropdown), 0);
  gtk_grid_attach_next_to(GTK_GRID(scanner_grid), n->scan_dropdown, n->scan_label, GTK_POS_RIGHT, 1, 1);
  g_signal_connect(n->scan_dropdown, "activate", G_CALLBACK(print_lol), NULL);
  gtk_widget_set_halign (n->scan_dropdown, GTK_ALIGN_START);
  gtk_widget_set_valign (n->scan_dropdown, GTK_ALIGN_START);
  gtk_widget_set_margin_end(n->scan_dropdown, 15);
  gtk_widget_set_margin_start(n->scan_dropdown, 15);

  gtk_grid_attach_next_to(GTK_GRID(scanner_grid), n->scan_remove, n->scan_dropdown, GTK_POS_RIGHT, 1, 1);
  gtk_widget_set_valign (n->scan_remove, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (n->scan_remove, GTK_ALIGN_CENTER);
  g_signal_connect(n->scan_remove, "clicked", G_CALLBACK(remove_scan), NULL);

  total_price();
}

static void activate (GtkApplication *app, gpointer user_data)
{

  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *stack;
  GtkWidget *stack_switcher;
  GtkWidget *separator;
  GtkWidget *scanner_frame;
  GtkWidget *scanner_entry;
  GtkEntryBuffer *scanner_entry_buffer;
  GtkWidget *submit_button;
  GtkWidget *cancel_button;
  GtkWidget *inventory_frame;
  GtkWidget *inventory_scroll;
  GtkWidget *button;
  GtkWidget *button2;
  GtkWidget *button3;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 900, 900);

  grid = gtk_grid_new();
  gtk_window_set_child(GTK_WINDOW(window), grid);

  stack = gtk_stack_new();
  gtk_grid_attach(GTK_GRID(grid), stack, 1, 3, 1, 1);

  stack_switcher = gtk_stack_switcher_new();
  gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stack_switcher), GTK_STACK(stack));
  gtk_grid_attach(GTK_GRID(grid), stack_switcher, 1, 1, 1, 1);

  scanner_grid = gtk_grid_new();
  gtk_widget_set_hexpand(scanner_grid, TRUE);
  gtk_widget_set_vexpand(scanner_grid, TRUE);
  gtk_grid_set_row_spacing(GTK_GRID(scanner_grid), 10);

  scanner_frame = gtk_frame_new(NULL);
  gtk_frame_set_child(GTK_FRAME(scanner_frame), scanner_grid);
  gtk_stack_add_titled(GTK_STACK(stack), scanner_frame, "scanner", "Scanner");
  gtk_widget_set_margin_top(scanner_frame, 15);
  gtk_widget_set_margin_bottom(scanner_frame, 15);
  gtk_widget_set_margin_start(scanner_frame, 15);
  gtk_widget_set_margin_end(scanner_frame, 15);

  scanner_entry = gtk_entry_new();
  gtk_grid_attach(GTK_GRID(grid), scanner_entry, 1, 2, 1, 1);
  g_signal_connect(scanner_entry, "activate", G_CALLBACK(add_scan), NULL);
  gtk_widget_set_halign (scanner_entry, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (scanner_entry, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand(scanner_entry, TRUE);
  gtk_widget_set_margin_top(scanner_entry, 15);
  gtk_widget_grab_focus(scanner_entry);

  scanner_entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(scanner_entry));
  scanner_entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(scanner_entry));

  separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);

  inventory_grid = gtk_grid_new();
  gtk_widget_set_halign (inventory_grid, GTK_ALIGN_START);
  gtk_widget_set_valign (inventory_grid, GTK_ALIGN_START);

  inventory_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_halign (inventory_box, GTK_ALIGN_START);
  gtk_widget_set_valign (inventory_box, GTK_ALIGN_START);

  inventory_scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(inventory_scroll), inventory_box);

  inventory_frame = gtk_frame_new(NULL);
  gtk_stack_add_titled(GTK_STACK(stack), inventory_frame, "inventory", "Inventario");
  gtk_frame_set_child(GTK_FRAME(inventory_frame), inventory_scroll);
  gtk_widget_set_margin_top(inventory_frame, 15);
  gtk_widget_set_margin_bottom(inventory_frame, 15);
  gtk_widget_set_margin_start(inventory_frame, 15);
  gtk_widget_set_margin_end(inventory_frame, 15);

  submit_button = gtk_button_new_with_label("     OK     ");
  gtk_grid_attach(GTK_GRID(scanner_grid), submit_button, 15, 100, 1, 1);
  g_signal_connect(submit_button, "clicked", G_CALLBACK(sql_sell), NULL);
  g_signal_connect_swapped(submit_button, "clicked", G_CALLBACK(free_scanner), scanner_entry);
  gtk_widget_set_halign (submit_button, GTK_ALIGN_END);
  gtk_widget_set_valign (submit_button, GTK_ALIGN_END);
  gtk_widget_set_vexpand(submit_button, TRUE);
  gtk_widget_set_margin_top(submit_button, 15);
  gtk_widget_set_margin_bottom(submit_button, 15);
  gtk_widget_set_margin_end(submit_button, 15);

  cancel_button = gtk_button_new_with_label("Cancelar");
  gtk_grid_attach_next_to(GTK_GRID(scanner_grid), cancel_button, submit_button, GTK_POS_LEFT, 1, 1);
  g_signal_connect_swapped(cancel_button, "clicked", G_CALLBACK(free_scanner), scanner_entry);
  gtk_widget_set_halign (cancel_button, GTK_ALIGN_END);
  gtk_widget_set_valign (cancel_button, GTK_ALIGN_END);
  gtk_widget_set_vexpand(cancel_button, TRUE);
  gtk_widget_set_margin_top(cancel_button, 15);
  gtk_widget_set_margin_bottom(cancel_button, 15);
  gtk_widget_set_margin_end(cancel_button, 15);

  price_total = gtk_label_new("Total: $");
  gtk_grid_attach_next_to(GTK_GRID(scanner_grid), price_total, cancel_button, GTK_POS_LEFT, 1, 1);
  gtk_widget_set_halign (price_total, GTK_ALIGN_END);
  gtk_widget_set_valign (price_total, GTK_ALIGN_END);
  gtk_widget_set_vexpand(price_total, TRUE);
  gtk_widget_set_hexpand(price_total, TRUE);
  gtk_widget_set_margin_top(price_total, 15);
  gtk_widget_set_margin_bottom(price_total, 23);
  gtk_widget_set_margin_end(price_total, 15);

  sql_query_example();
  sql_inventory();
  gtk_window_present (GTK_WINDOW (window));
}

int main (int argc, char **argv)
{
  rc = sqlite3_open("inventory.db", &db);
  if (rc) {
    fprintf(stderr, "Can't open databse: %s\n", sqlite3_errmsg(db));
    return 0;
  }
  else {
    fprintf(stdout, "Opened database succesfully\n");
  }

  GtkApplication *app;
  int status;

  app = gtk_application_new ("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return status;
}

