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
#include <time.h>

typedef struct node{
  const char *barcode;
  const char *name;
  const char *price;
  const char *type;
  const char *brand;
  const char *id;
  int quantity;
  int total;
  int quantity_sold;
  GtkWidget *scan_label;
  GtkWidget *scan_dropdown;
  const char **quantity_dropdown;
  GtkWidget *scan_remove;
  struct node *next;
} node;

node *scanner_list = NULL;
GtkWidget *scanner_grid;
GtkWidget *price_total;
GtkWidget *scanner_entry;
const char *scanner_entry_text;
const char *inventory_entry_text;
char *price_total_default = "Total: $";
int scanner_list_n_of_products = 0;
int total_price = 0;

GtkWidget *inventory_grid;
GtkWidget *inventory_box;

GtkWidget *floating_window_grid;
GtkWidget *floating_window_cancel_button;
GtkWidget *floating_window_submit_button;

GtkWidget *history_box;
GtkWidget *history_calendar;

sqlite3 *db;
const char *errMsg = 0;
sqlite3_stmt *stmt;
int rc; 

static void history_query();

static char* sql_query(const char *barcode, const char *sql){
  char *query = sqlite3_mprintf("SELECT %s FROM products WHERE barcode = ?", sql);
  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  rc = sqlite3_bind_text(stmt, 1, barcode, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
    return NULL;
  }

  char *result = NULL; 

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

  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Error fetching rows: %s\n", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);

  free(query);
  return result;
}

static void update_inventory(GtkWidget* widget, char *type){
  
  const char *barcode = inventory_entry_text;
  char *query = sqlite3_mprintf("UPDATE products set %s = ? WHERE barcode = ?", type);

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL); 
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return;
  }

  rc = sqlite3_bind_text(stmt, 1, gtk_editable_get_text(GTK_EDITABLE(widget)), -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc = sqlite3_bind_text(stmt, 2, barcode, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
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

  g_signal_connect(price_label, "changed", G_CALLBACK(update_inventory), "price");

  const char *quantity_sql = (const char*)sqlite3_column_text(stmt, 4);
  GtkWidget *quantity_label = gtk_editable_label_new(quantity_sql);
  gtk_box_append(GTK_BOX(inventory_product_box), quantity_label);


  g_signal_connect(quantity_label, "changed", G_CALLBACK(update_inventory), "quantity");
}

static void sql_inventory(){
  char *query = "SELECT type, brand, name, barcode, quantity, price FROM products ORDER BY type, brand;";

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
    int col_count = sqlite3_column_count(stmt);
    

    if (type_sql != NULL && type_sql_temp != NULL && strcmp(type_sql, type_sql_temp) == 0) {
      if (strcmp(brand_sql, brand_sql_temp) == 0) {
        sql_inventory_product(inventory_brand, inventory_products_box);
        continue; 
      }
      inventory_brand = gtk_expander_new_with_mnemonic(brand_sql);
      gtk_expander_set_use_markup(GTK_EXPANDER(inventory_brand), TRUE);
      gtk_box_append(GTK_BOX(inventory_brand_box), inventory_brand);

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
    type_sql_temp = malloc((strlen(type_sql) + 1) * sizeof(char));
    strcpy(type_sql_temp, type_sql);
    GtkWidget *inventory_type = gtk_expander_new_with_mnemonic(type_sql);
    gtk_expander_set_use_markup(GTK_EXPANDER(inventory_type), TRUE);

    brand_sql = (const char*)sqlite3_column_text(stmt, 1);
    brand_sql_temp = malloc(( strlen(brand_sql) * sizeof(char) ) + 1);
    strcpy(brand_sql_temp, brand_sql);

    inventory_brand = gtk_expander_new_with_mnemonic(brand_sql);
    gtk_expander_set_use_markup(GTK_EXPANDER(inventory_brand), TRUE);

    gtk_box_append(GTK_BOX(inventory_box), inventory_type);
    gtk_expander_set_child(GTK_EXPANDER(inventory_type), inventory_brand_box);
    gtk_box_append(GTK_BOX(inventory_brand_box), inventory_brand);

    inventory_products_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
    sql_inventory_product(inventory_brand, inventory_products_box);
    
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
  }

  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Error fetching rows: %s\n", sqlite3_errmsg(db));
  }
  return result;
}


static int calc_total_price(){
  total_price = 0;
  for (node *ptr = scanner_list; ptr != NULL; ptr = ptr->next) {
    GtkStringObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(ptr->scan_dropdown));
    const char* item_value = gtk_string_object_get_string(item);
    int product_total = (atoi(ptr->price) * atoi(item_value));
    ptr->total = product_total;
    total_price += product_total;
  }

  char total_str[50];
  sprintf(total_str, "%s%d", price_total_default, total_price);
  gtk_label_set_text(GTK_LABEL(price_total), total_str);
  return total_price;
}

static void sql_sell_update_products(const char *id, int quantity){
  char *query = "UPDATE products SET quantity = ? WHERE id = ?";

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL); 
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return;
  }

  rc = sqlite3_bind_int(stmt, 1, quantity);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc = sqlite3_bind_text(stmt, 2, id, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db)); 
  }
  else {
    printf("Update succesfully\n");
  }

  sqlite3_finalize(stmt);
}

static void receipt_print(struct tm *date){
  char receipt[2000];
  sprintf(receipt, 
    "Distribuidora Cabellos risos\n" 
    "Nit: 39549232-1\n" 
    "Calle 70c #70-06\n" 
    "--------------------------\n" 
    "Fecha: %d-%d-%d\n"
    "Hora: %d:%d\n"  
    "--------------------------\n", date->tm_year, date->tm_mon, date->tm_mday, date->tm_hour, date->tm_min
  );

  for (node *ptr = scanner_list; ptr != NULL; ptr = ptr->next) {
    sprintf(receipt + strlen(receipt),     
      "%s %s\n%s\n(%d): $%d\n", ptr->type, ptr->brand, ptr->name, ptr->quantity_sold, ptr->total
    );
  }

  int net_price = total_price / 1.19;
  sprintf(receipt + strlen(receipt), 
    "---------------------------\n"    
    "Precio Neto: $%d\n"
    "IVA(19%%): $%d\n"
    "Precio Total: $%d", net_price, total_price - net_price, total_price  
  );

  FILE *fp = popen("lpr", "w");
  if (fp == NULL) {
    perror("failed to run lpr"); 
  }
  fprintf(fp, "%s", receipt);
  g_print("we dit itttttt\n");
  fclose(fp);
}

static struct tm *current_time(){
  time_t t = time(NULL);
  struct tm *cur_time = localtime(&t);
  cur_time->tm_year += 1900;
  cur_time->tm_mon += 1;
  return cur_time;
}

static int sql_sell_insert_sales(struct tm *cur_time){
  char *query = "INSERT INTO sales (year, month, day, time, total_price, weekday) VALUES (?, ?, ?, ?, ?, ?)";

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  char time[20];
  sprintf(time, "%d:%d", cur_time->tm_hour, cur_time->tm_min);

  rc = sqlite3_bind_int(stmt, 1, cur_time->tm_year);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }
  rc = sqlite3_bind_int(stmt, 2, cur_time->tm_mon);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }
  rc = sqlite3_bind_int(stmt, 3, cur_time->tm_mday);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }
  rc = sqlite3_bind_text(stmt, 4, time, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }
  rc = sqlite3_bind_int(stmt, 5, total_price);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }
  rc = sqlite3_bind_int(stmt, 6, cur_time->tm_wday);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db)); 
  }
  else {
    printf("Update succesfully\n");
  }

  int id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return id;
}

static void sql_sell_insert_sales_items(const char *product_id, int sale_id, int quantity, int unit_price){
  char * query = "INSERT INTO sales_items (product_id, sale_id, quantity, unit_price) VALUES (?, ?, ?, ?)";

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return;
  }

  rc = sqlite3_bind_int(stmt, 1, atoi(product_id));
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }
  rc = sqlite3_bind_int(stmt, 2, sale_id);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }
  rc = sqlite3_bind_int(stmt, 3, quantity);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }
  rc = sqlite3_bind_int(stmt, 4, unit_price);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db)); 
  }
  else {
    printf("Update succesfully\n");
  }

  sqlite3_finalize(stmt);
}

static void sql_sell(GtkWidget *widget, gpointer data){
  if (total_price == 0) {
    return; 
  }
  struct tm *cur_time = current_time();
  int sales_id = sql_sell_insert_sales(cur_time);
  for (node *ptr = scanner_list; ptr != NULL; ptr = ptr->next) {
    GtkStringObject *item_quantity = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(ptr->scan_dropdown));
    const char* item_quantity_value = gtk_string_object_get_string(item_quantity);

    ptr->quantity_sold = atoi(item_quantity_value);
    int updated_quantity = ptr->quantity - ptr->quantity_sold;
    int unit_price = atoi(ptr->price);

    sql_sell_update_products(ptr->id, updated_quantity);
    sql_sell_insert_sales_items(ptr->id, sales_id, ptr->quantity_sold, unit_price);
  }
  receipt_print(cur_time);
  gtk_label_set_text(GTK_LABEL(price_total), "Total: ");
  history_query();
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
    free((void *)ptr->id);

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
    calc_total_price();
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
      free((void *)temp->name);
      free((void *)temp->brand);
      free((void *)temp->type);
      free((void *)temp->price);
      free(temp);
      calc_total_price();
      return;
    } 
    ptr = ptr->next;
  }
}

static void add_scan(GtkWidget *widget, gpointer data)
{
  GtkEntryBuffer *scanner_entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(widget));
  const char *scan = g_strdup(gtk_entry_buffer_get_text(scanner_entry_buffer));
  gtk_entry_buffer_set_text(scanner_entry_buffer, "", 0);

  char *sql_quantity = sql_query(scan, "quantity");
  if (sql_quantity == NULL || atoi(sql_quantity) == 0) {
    return; 
  }

  int quantity = atoi(sql_quantity);

  if (scanner_list != NULL ) {   
    for (node *ptr = scanner_list; ptr != NULL; ptr = ptr->next) {
      if (strcmp(ptr->barcode, scan) == 0) {
        GtkStringObject *selected_quantity = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(ptr->scan_dropdown));
        const char* selected_quantity_value = gtk_string_object_get_string(selected_quantity);

        if (atoi(selected_quantity_value) == quantity) {
          return; 
        }
        guint n = atoi(selected_quantity_value);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ptr->scan_dropdown), n);
        calc_total_price();
        return;
      }
    }
  }
  scanner_list_n_of_products++;

  const char *name = (const char *)sql_query(scan, "name");
  if (name == NULL) {
    return; 
  }
  const char *brand = sql_query(scan, "brand");
  const char *type = sql_query(scan, "type");
  const char *price = sql_query(scan, "price");
  const char *id = sql_query(scan, "id");

  char label[100];
  sprintf(label, "%s %s %s\t$%s", type, brand, name, price);

  const char* quantity_dropdown[quantity + 1];
  for (int i = 0; i < quantity; i++) {
    quantity_dropdown[i] = malloc((3 + 1) * sizeof(char));
    sprintf((char *)quantity_dropdown[i], "%d", i + 1);
  }
  quantity_dropdown[quantity] = NULL;

  node *n = malloc(sizeof(node));
  n->barcode = scan;
  n->brand = brand;
  n->name = name;  
  n->type = type;
  n->id = id;
  n->scan_label = gtk_label_new(label);
  n->quantity_dropdown = quantity_dropdown;
  n->scan_dropdown = gtk_drop_down_new_from_strings(quantity_dropdown);
  n->scan_remove = gtk_button_new_with_label("Eliminar");
  n->quantity = quantity;
  n->price = price;
  n->next = scanner_list;
  scanner_list = n;


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
  gtk_widget_set_halign (n->scan_dropdown, GTK_ALIGN_START);
  gtk_widget_set_valign (n->scan_dropdown, GTK_ALIGN_START);
  gtk_widget_set_margin_end(n->scan_dropdown, 15);
  gtk_widget_set_margin_start(n->scan_dropdown, 15);
  g_signal_connect(n->scan_dropdown, "notify::selected", G_CALLBACK(calc_total_price), NULL);
  g_signal_connect_swapped(n->scan_dropdown, "notify::selected", G_CALLBACK(gtk_widget_grab_focus), scanner_entry);
  gtk_grid_attach_next_to(GTK_GRID(scanner_grid), n->scan_remove, n->scan_dropdown, GTK_POS_RIGHT, 1, 1);
  gtk_widget_set_valign (n->scan_remove, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (n->scan_remove, GTK_ALIGN_CENTER);
  g_signal_connect(n->scan_remove, "clicked", G_CALLBACK(remove_scan), NULL);
  g_signal_connect_swapped(n->scan_remove, "clicked", G_CALLBACK(gtk_widget_grab_focus), scanner_entry);

  calc_total_price();
}

static void floating_window_new(GtkWidget *window){
  window = gtk_window_new(); 
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_window_set_child(GTK_WINDOW(window), box);

  GtkWidget *frame = gtk_frame_new(NULL);
  gtk_box_append(GTK_BOX(box), frame);
  gtk_widget_set_margin_top(frame, 15);
  gtk_widget_set_margin_bottom(frame, 15);
  gtk_widget_set_margin_start(frame, 15);
  gtk_widget_set_margin_end(frame, 15);
  gtk_widget_set_vexpand(frame, TRUE);
  gtk_widget_set_hexpand(frame, TRUE);

  floating_window_grid = gtk_grid_new();
  gtk_frame_set_child(GTK_FRAME(frame), floating_window_grid);
  gtk_widget_set_margin_start(floating_window_grid, 15);
  gtk_widget_set_margin_end(floating_window_grid, 15);
  gtk_widget_set_margin_top(floating_window_grid, 15);
  gtk_widget_set_margin_bottom(floating_window_grid, 15);

  GtkWidget *buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_box_append(GTK_BOX(box), buttons_box);

  floating_window_cancel_button = gtk_button_new_with_label("Cancelar");
  gtk_box_append(GTK_BOX(buttons_box), floating_window_cancel_button);
  gtk_widget_set_margin_start(floating_window_cancel_button, 135);
  gtk_widget_set_margin_end(floating_window_cancel_button, 15);
  gtk_widget_set_margin_bottom(floating_window_cancel_button, 15);
  g_signal_connect_swapped(floating_window_cancel_button, "clicked", G_CALLBACK(gtk_window_destroy), window);

  floating_window_submit_button = gtk_button_new_with_label("   OK   ");
  gtk_box_append(GTK_BOX(buttons_box), floating_window_submit_button);
  gtk_widget_set_margin_end(floating_window_submit_button, 15);
  gtk_widget_set_margin_bottom(floating_window_submit_button, 15);
  g_signal_connect_swapped(floating_window_submit_button, "clicked", G_CALLBACK(gtk_window_destroy), window);

  gtk_window_present (GTK_WINDOW(window));
}

static void calc_change(GtkWidget *paid_amount_entry, GtkWidget *change_label){
  const char *paid_amount = gtk_editable_get_text(GTK_EDITABLE(paid_amount_entry));
  int change = atoi(paid_amount) - total_price;
  char change_str[30];
  sprintf(change_str, "%d", change);
  gtk_label_set_text(GTK_LABEL(change_label), change_str);
}

static void floating_window_grid_calc_change(void){
  GtkWidget *total_label_1 = gtk_label_new("Total:");
  gtk_grid_attach(GTK_GRID(floating_window_grid), total_label_1, 1, 1, 1, 1);
  char total_price_text[30];
  sprintf(total_price_text, "$%d", total_price);
  GtkWidget *total_label_2 = gtk_label_new(total_price_text);
  gtk_grid_attach(GTK_GRID(floating_window_grid), total_label_2, 2, 1, 1, 1);

  GtkWidget *change_label_1 = gtk_label_new("Devuelta:");
  gtk_grid_attach(GTK_GRID(floating_window_grid), change_label_1, 1, 3, 1, 1);
  GtkWidget *change_label_2 = gtk_label_new("");
  gtk_grid_attach(GTK_GRID(floating_window_grid), change_label_2, 2, 3, 1, 1);

  GtkWidget *paid_amount_label = gtk_label_new("Paga con:");
  gtk_widget_set_margin_end(paid_amount_label, 15);
  gtk_grid_attach(GTK_GRID(floating_window_grid), paid_amount_label, 1, 2, 1, 1);
  GtkWidget *paid_amount_entry = gtk_entry_new();
  gtk_grid_attach(GTK_GRID(floating_window_grid), paid_amount_entry, 2, 2, 1, 1);
  g_signal_connect(paid_amount_entry, "changed", G_CALLBACK(calc_change), change_label_2);
  gtk_widget_grab_focus(paid_amount_entry);

  g_signal_connect_swapped(floating_window_cancel_button, "clicked", G_CALLBACK(gtk_widget_grab_focus), scanner_entry);

  g_signal_connect(floating_window_submit_button, "clicked", G_CALLBACK(sql_sell), NULL);
  g_signal_connect_swapped(floating_window_submit_button, "clicked", G_CALLBACK(gtk_widget_grab_focus), scanner_entry);
  g_signal_connect_swapped(floating_window_submit_button, "clicked", G_CALLBACK(free_scanner), scanner_entry);
}

static void insert_new_product(const char *barcode){
  char *query = "INSERT INTO products (type, brand, name, quantity, price, barcode) VALUES ('', '', '', 0, 0, ?)";

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return;
  }

  rc = sqlite3_bind_text(stmt, 1, barcode, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db)); 
  }
  else {
    printf("Update succesfully\n");
  }

  sqlite3_finalize(stmt);
}

static void floating_window_grid_edit_inventory(GtkWidget *inventory_entry){
  GtkEntryBuffer *inventory_entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(inventory_entry)); 
  inventory_entry_text = gtk_entry_buffer_get_text(inventory_entry_buffer);

  GtkWidget *barcode_label = gtk_label_new("Codigo:");
  gtk_grid_attach(GTK_GRID(floating_window_grid), barcode_label, 1, 1, 1, 1);
  GtkWidget *barcode_editable = gtk_editable_label_new(inventory_entry_text);
  gtk_grid_attach(GTK_GRID(floating_window_grid), barcode_editable, 2, 1, 1, 1);

  GtkWidget *type_label = gtk_label_new("Tipo:");
  gtk_grid_attach(GTK_GRID(floating_window_grid), type_label, 1, 2, 1, 1);
  GtkWidget *type_editable = gtk_editable_label_new("");
  gtk_grid_attach(GTK_GRID(floating_window_grid), type_editable, 2, 2, 1, 1);

  GtkWidget *brand_label = gtk_label_new("Marca:");
  gtk_grid_attach(GTK_GRID(floating_window_grid), brand_label, 1, 3, 1, 1);
  GtkWidget *brand_editable = gtk_editable_label_new("");
  gtk_grid_attach(GTK_GRID(floating_window_grid), brand_editable, 2, 3, 1, 1);

  GtkWidget *name_label = gtk_label_new("Nombre:");
  gtk_grid_attach(GTK_GRID(floating_window_grid), name_label, 1, 4, 1, 1);
  GtkWidget *name_editable = gtk_editable_label_new("");
  gtk_grid_attach(GTK_GRID(floating_window_grid), name_editable, 2, 4, 1, 1);

  GtkWidget *price_label = gtk_label_new("Precio");
  gtk_grid_attach(GTK_GRID(floating_window_grid), price_label, 1, 5, 1, 1);
  GtkWidget *price_editable = gtk_editable_label_new("");
  gtk_grid_attach(GTK_GRID(floating_window_grid), price_editable, 2, 5, 1, 1);

  GtkWidget *quantity_label = gtk_label_new("Cantidad:");
  gtk_grid_attach(GTK_GRID(floating_window_grid), quantity_label, 1, 6, 1, 1);
  GtkWidget *quantity_editable = gtk_editable_label_new("");
  gtk_grid_attach(GTK_GRID(floating_window_grid), quantity_editable, 2, 6, 1, 1);

  if (sql_query(inventory_entry_text, "barcode") == NULL) {
    insert_new_product(inventory_entry_text);
  }

  g_signal_connect(barcode_editable, "changed", G_CALLBACK(update_inventory), "barcode");

  gtk_editable_set_text(GTK_EDITABLE(type_editable), sql_query(inventory_entry_text, "type"));
  g_signal_connect(type_editable, "changed", G_CALLBACK(update_inventory), "type");

  gtk_editable_set_text(GTK_EDITABLE(brand_editable), sql_query(inventory_entry_text, "brand"));
  g_signal_connect(brand_editable, "changed", G_CALLBACK(update_inventory), "brand");

  gtk_editable_set_text(GTK_EDITABLE(name_editable), sql_query(inventory_entry_text, "name"));
  g_signal_connect(name_editable, "changed", G_CALLBACK(update_inventory), "name");

  gtk_editable_set_text(GTK_EDITABLE(price_editable), sql_query(inventory_entry_text, "price"));
  g_signal_connect(price_editable, "changed", G_CALLBACK(update_inventory), "price");

  gtk_editable_set_text(GTK_EDITABLE(quantity_editable), sql_query(inventory_entry_text, "quantity"));
  g_signal_connect(quantity_editable, "changed", G_CALLBACK(update_inventory), "quantity");
}

static void history_query_sale_items(GtkWidget *expander, int id){

  char *query = "SELECT products.type, products.brand, products.name, sales_items.quantity, products.price FROM ((products JOIN sales_items ON products.id = sales_items.product_id) JOIN sales ON sales_items.sale_id = sales.id) WHERE sales.day = ? AND sales.month = ? AND sales.year = ? AND sales.id = ?";

  int rc_2;
  sqlite3_stmt *stmt_2;
  rc_2 = sqlite3_prepare_v2(db, query, -1, &stmt_2, NULL);
  if (rc_2 != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return;
  }

  rc_2 = sqlite3_bind_int(stmt_2, 1, gtk_calendar_get_day(GTK_CALENDAR(history_calendar)));
  if (rc_2 != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc_2 = sqlite3_bind_int(stmt_2, 2, gtk_calendar_get_month(GTK_CALENDAR(history_calendar)) + 1);
  if (rc_2 != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc_2 = sqlite3_bind_int(stmt_2, 3, gtk_calendar_get_year(GTK_CALENDAR(history_calendar)));
  if (rc_2 != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc_2 = sqlite3_bind_int(stmt_2, 4, id);
  if (rc_2 != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  GtkWidget *item_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  g_print("we goood\n");
  gtk_expander_set_child(GTK_EXPANDER(expander), item_box);
  while ((rc_2 = sqlite3_step(stmt_2)) == SQLITE_ROW) {
    const char *col_type = (const char*)sqlite3_column_text(stmt_2, 0);
    const char *col_brand = (const char*)sqlite3_column_text(stmt_2, 1);
    const char *col_name = (const char*)sqlite3_column_text(stmt_2, 2);
    const char *col_quantity = (const char*)sqlite3_column_text(stmt_2, 3);
    const char *col_price = (const char*)sqlite3_column_text(stmt_2, 4);
    char full_item_name[100];
    sprintf(full_item_name, "%s %s %s x %s = %s", col_type, col_brand, col_name, col_quantity, col_price);
    GtkWidget *sold_item = gtk_label_new(full_item_name);
    gtk_box_append(GTK_BOX(item_box), sold_item);
  }

  if (rc_2 != SQLITE_DONE) {
    fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db)); 
  }
  else {
    printf("Update succesfully\n");
  }

  sqlite3_finalize(stmt_2);
}


static void history_query(){
  if (history_box == NULL) {
    return; 
  }

  GtkWidget *child = gtk_widget_get_first_child(history_box);

  while (child != NULL) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_box_remove(GTK_BOX(history_box), child); 
    child = next;
  }
  
  char *query = "SELECT time, total_price, id FROM sales WHERE day = ? AND month = ? AND year = ? ORDER BY id";

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    return;
  }

  rc = sqlite3_bind_int(stmt, 1, gtk_calendar_get_day(GTK_CALENDAR(history_calendar)));
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc = sqlite3_bind_int(stmt, 2, gtk_calendar_get_month(GTK_CALENDAR(history_calendar)) + 1);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  rc = sqlite3_bind_int(stmt, 3, gtk_calendar_get_year(GTK_CALENDAR(history_calendar)));
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind: %s\n", sqlite3_errmsg(db)); 
  }

  char *time;
  char *total_price;

  int total_sold_today = 0;

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const char *col_time = (const char*)sqlite3_column_text(stmt, 0);
    const char *col_total_price = (const char*)sqlite3_column_text(stmt, 1);
    char history_sale[50];
    sprintf(history_sale, "$%s - %s", col_total_price, col_time);
    GtkWidget *history_expander = gtk_expander_new_with_mnemonic(history_sale);
    gtk_box_append(GTK_BOX(history_box), history_expander);

    int col_id = sqlite3_column_int(stmt, 2);
    history_query_sale_items(history_expander, col_id);

    total_sold_today += atoi(col_total_price);
  }

  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db)); 
  }
  else {
    printf("Update succesfully\n");
  }

  char total_sold_today_str[40];
  sprintf(total_sold_today_str, "Total diario: $%d", total_sold_today);
  GtkWidget *total_sold_today_label = gtk_label_new(total_sold_today_str);
  gtk_box_append(GTK_BOX(history_box), total_sold_today_label);

  sqlite3_finalize(stmt);
}

static void activate (GtkApplication *app, gpointer user_data){
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *stack;
  GtkWidget *stack_switcher;
  GtkWidget *separator;
  GtkWidget *scanner_stack_box;
  GtkWidget *scanner_frame;
  GtkEntryBuffer *scanner_entry_buffer;
  GtkWidget *submit_button;
  GtkWidget *cancel_button;

  GtkWidget *inventory_stack_box;
  GtkWidget *inventory_frame;
  GtkWidget *inventory_scroll;
  GtkWidget *inventory_entry;
  GtkEntryBuffer *inventory_entry_buffer;

  GtkWidget *inventory_product_window;
  GtkWidget *calc_change_window;

  GtkWidget *history_stack_box;
  GtkWidget *history_calendar_frame;
  GtkWidget *history_frame;
  GtkWidget *history_scrolled;

  GtkWidget *frame_box;
  GtkWidget *buttons_box;

  GtkCssProvider *css_provider = gtk_css_provider_new();
  GFile *css_file = g_file_new_for_path("inventory.css"); 
  gtk_css_provider_load_from_file(css_provider, css_file);
  gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

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

  scanner_stack_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_stack_add_titled(GTK_STACK(stack), scanner_stack_box, "scanner", "Scanner");

  scanner_entry = gtk_entry_new();
  gtk_box_append(GTK_BOX(scanner_stack_box), scanner_entry);
  g_signal_connect(scanner_entry, "activate", G_CALLBACK(add_scan), NULL);
  gtk_widget_set_halign (scanner_entry, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (scanner_entry, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand(scanner_entry, TRUE);
  gtk_widget_set_margin_top(scanner_entry, 15);
  gtk_widget_grab_focus(scanner_entry);

  scanner_entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(scanner_entry));


  frame_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  scanner_frame = gtk_frame_new(NULL);
  gtk_box_append(GTK_BOX(scanner_stack_box), scanner_frame); 
  gtk_frame_set_child(GTK_FRAME(scanner_frame), frame_box);
  gtk_widget_set_margin_top(scanner_frame, 15);
  gtk_widget_set_margin_bottom(scanner_frame, 15);
  gtk_widget_set_margin_start(scanner_frame, 15);
  gtk_widget_set_margin_end(scanner_frame, 15);
  gtk_box_append(GTK_BOX(frame_box), scanner_grid);


  separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);

  inventory_grid = gtk_grid_new();
  gtk_widget_set_halign (inventory_grid, GTK_ALIGN_START);
  gtk_widget_set_valign (inventory_grid, GTK_ALIGN_START);

  inventory_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_halign (inventory_box, GTK_ALIGN_START);
  gtk_widget_set_valign (inventory_box, GTK_ALIGN_START);

  inventory_scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(inventory_scroll), inventory_box);

  inventory_stack_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_stack_add_titled(GTK_STACK(stack), inventory_stack_box, "inventory", "Inventario");
  gtk_widget_set_vexpand(inventory_stack_box, TRUE);

  inventory_entry = gtk_entry_new();
  gtk_box_append(GTK_BOX(inventory_stack_box), inventory_entry);
  g_signal_connect_swapped(inventory_entry, "activate", G_CALLBACK(floating_window_new), inventory_product_window);
  g_signal_connect(inventory_entry, "activate", G_CALLBACK(floating_window_grid_edit_inventory), NULL);

  gtk_widget_set_halign (inventory_entry, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (inventory_entry, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand(inventory_entry, TRUE);
  gtk_widget_set_margin_top(inventory_entry, 15);

  inventory_frame = gtk_frame_new(NULL);
  gtk_box_append(GTK_BOX(inventory_stack_box), inventory_frame);
  gtk_frame_set_child(GTK_FRAME(inventory_frame), inventory_scroll);
  gtk_widget_set_margin_top(inventory_frame, 15);
  gtk_widget_set_margin_bottom(inventory_frame, 15);
  gtk_widget_set_margin_start(inventory_frame, 15);
  gtk_widget_set_margin_end(inventory_frame, 15);
  gtk_widget_set_vexpand(inventory_frame, TRUE);

  history_stack_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_stack_add_titled(GTK_STACK(stack), history_stack_box, "history", "Historial");

  history_calendar = gtk_calendar_new();
  gtk_box_append(GTK_BOX(history_stack_box), history_calendar);
  gtk_widget_set_margin_top(history_calendar, 15);
  gtk_widget_set_margin_bottom(history_calendar, 15);
  g_signal_connect(history_calendar, "day-selected", G_CALLBACK(history_query), NULL);

  history_frame = gtk_frame_new(NULL);
  gtk_box_append(GTK_BOX(history_stack_box), history_frame);
  gtk_widget_set_margin_top(history_frame, 15);
  gtk_widget_set_margin_bottom(history_frame, 15);
  gtk_widget_set_margin_start(history_frame, 15);
  gtk_widget_set_margin_end(history_frame, 15);
  gtk_widget_set_vexpand(history_frame, TRUE);
  gtk_widget_set_hexpand(history_frame, TRUE);

  history_scrolled = gtk_scrolled_window_new();
  gtk_frame_set_child(GTK_FRAME(history_frame), history_scrolled);

  history_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(history_scrolled), history_box);

  buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_box_append(GTK_BOX(frame_box), buttons_box);

  price_total = gtk_label_new("Total: $");
  gtk_box_append(GTK_BOX(buttons_box), price_total);
  gtk_widget_set_halign (price_total, GTK_ALIGN_END);
  gtk_widget_set_valign (price_total, GTK_ALIGN_END);
  gtk_widget_set_vexpand(price_total, TRUE);
  gtk_widget_set_hexpand(price_total, TRUE);
  gtk_widget_set_margin_top(price_total, 15);
  gtk_widget_set_margin_end(price_total, 15);
  gtk_widget_set_margin_bottom(price_total, 23);

  cancel_button = gtk_button_new_with_label("Cancelar");
  gtk_box_append(GTK_BOX(buttons_box), cancel_button);
  g_signal_connect_swapped(cancel_button, "clicked", G_CALLBACK(free_scanner), scanner_entry);
  gtk_widget_set_halign (cancel_button, GTK_ALIGN_END);
  gtk_widget_set_valign (cancel_button, GTK_ALIGN_END);
  gtk_widget_set_vexpand(cancel_button, TRUE);
  gtk_widget_set_margin_top(cancel_button, 15);
  gtk_widget_set_margin_bottom(cancel_button, 15);
  gtk_widget_set_margin_end(cancel_button, 15);

  submit_button = gtk_button_new_with_label("     OK     ");
  gtk_box_append(GTK_BOX(buttons_box), submit_button);
  g_signal_connect_swapped(submit_button, "clicked", G_CALLBACK(floating_window_new), calc_change_window);
  g_signal_connect(submit_button, "clicked", G_CALLBACK(floating_window_grid_calc_change), NULL);
  gtk_widget_set_halign (submit_button, GTK_ALIGN_END);
  gtk_widget_set_valign (submit_button, GTK_ALIGN_END);
  gtk_widget_set_vexpand(submit_button, TRUE);
  gtk_widget_set_margin_top(submit_button, 15);
  gtk_widget_set_margin_bottom(submit_button, 15);
  gtk_widget_set_margin_end(submit_button, 15);



  sql_inventory();
  history_query();
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

