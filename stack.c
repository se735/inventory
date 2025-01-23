#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkdropdown.h"
#include "gtk/gtkshortcut.h"
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

static int callback(void *NotUsed, int argc, char **argv, char **azColName); 

typedef struct node{
  const char *barcode;
  GtkWidget *scan_label;
  GtkWidget *scan_dropdown;
  struct node *next;
} node;

node *scanner_list = NULL;
GtkWidget *scanner_grid;

const char *numbers[101];

static void print_hello (GtkWidget *widget, gpointer data)
{
  g_print ("Hello World\n");
}

static void print_dropdown(GtkWidget *widget, gpointer data){
  GtkStringObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(widget));
  const char* item_value = gtk_string_object_get_string(item);
  g_print("%s", item_value);
  g_print("lol");
}

static void add_scan(GtkWidget *widget, gpointer data)
{
  if (numbers[0] == NULL) {
    for (int i = 0; i < 100; i++) {
      char *str = malloc(3);
      sprintf(str, "%d", i);
      numbers[i] = str;
    }
  }

  GtkEntryBuffer *scanner_entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(widget));
  const char *scan =   g_strdup(gtk_entry_buffer_get_text(scanner_entry_buffer));
  gtk_entry_buffer_set_text(scanner_entry_buffer, "", 0);

  node *n = malloc(sizeof(node));
  n->barcode = scan;
  n->scan_label = gtk_label_new(scan);
  n->scan_dropdown = gtk_drop_down_new_from_strings(numbers);
  n->next = scanner_list;
  scanner_list = n;

  if (n->next == NULL) {
    gtk_grid_attach(GTK_GRID(scanner_grid), n->scan_label, 1, 2, 1, 1);
  }
  else {
    gtk_grid_attach_next_to(GTK_GRID(scanner_grid), n->scan_label, n->next->scan_label, GTK_POS_BOTTOM, 1, 1);
  }
  gtk_widget_set_halign (n->scan_label, GTK_ALIGN_START);
  gtk_widget_set_valign (n->scan_label, GTK_ALIGN_START);

  gtk_grid_attach_next_to(GTK_GRID(scanner_grid), n->scan_dropdown, n->scan_label, GTK_POS_RIGHT, 1, 1);
  g_signal_connect(n->scan_dropdown, "activate", G_CALLBACK(print_dropdown), NULL);

  if (n->next != NULL) {
    GtkStringObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(n->next->scan_dropdown));
    const char* item_value = gtk_string_object_get_string(item);
    g_print("%s", item_value);
  }
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
  GtkWidget *inventory_box;
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

  scanner_frame = gtk_frame_new(NULL);
  gtk_frame_set_child(GTK_FRAME(scanner_frame), scanner_grid);
  gtk_stack_add_titled(GTK_STACK(stack), scanner_frame, "scanner", "Scanner");
  gtk_widget_set_margin_top(scanner_frame, 15);
  gtk_widget_set_margin_bottom(scanner_frame, 15);
  gtk_widget_set_margin_start(scanner_frame, 15);
  gtk_widget_set_margin_end(scanner_frame, 15);

  scanner_entry = gtk_entry_new();
  gtk_grid_attach(GTK_GRID(scanner_grid), scanner_entry, 1, 1, 1, 1);
  g_signal_connect(scanner_entry, "activate", G_CALLBACK(add_scan), NULL);
  gtk_widget_set_halign (scanner_entry, GTK_ALIGN_START);
  gtk_widget_set_valign (scanner_entry, GTK_ALIGN_START);

  scanner_entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(scanner_entry));
  scanner_entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(scanner_entry));

  separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);

  inventory_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_stack_add_titled(GTK_STACK(stack), inventory_box, "inventory", "Inventario");
  gtk_widget_set_halign (inventory_box, GTK_ALIGN_START);
  gtk_widget_set_valign (inventory_box, GTK_ALIGN_START);

  button = gtk_button_new_with_label ("Hello World");
  g_signal_connect (button, "clicked", G_CALLBACK (print_hello), NULL);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_window_destroy), window);

  button2 = gtk_button_new_with_label ("Hello World");
  g_signal_connect (button2, "clicked", G_CALLBACK (print_hello), NULL);
  g_signal_connect_swapped (button2, "clicked", G_CALLBACK (gtk_window_destroy), window);

  button3 = gtk_button_new_with_label ("Hello World");
  g_signal_connect (button3, "clicked", G_CALLBACK (print_hello), NULL);
  g_signal_connect_swapped (button3, "clicked", G_CALLBACK (gtk_window_destroy), window);
  gtk_box_append (GTK_BOX (inventory_box), button3);

  gtk_window_present (GTK_WINDOW (window));
}

int main (int argc, char **argv)
{
  numbers[0] = NULL;
  sqlite3 *db;
  char *zErrMsg = 0;
  int rc;
  char *sql;

  rc = sqlite3_open("inventory.db", &db);

  if (rc) {
    fprintf(stderr, "Can't open databse: %s\n", sqlite3_errmsg(db));
    return 0;
  }
  else {
    fprintf(stdout, "Opened database succesfully\n");
  }

  printf("ass");


  rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
  if( rc != SQLITE_OK ) {
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } 
  else {
    fprintf(stdout, "Table created successfully\n");
  }

  sqlite3_close(db);

  GtkApplication *app;
  int status;

  app = gtk_application_new ("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}
