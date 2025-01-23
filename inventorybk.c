#include "gtk/gtk.h"
#include <stdio.h>
#include <sqlite3.h>

static int callback(void *NotUsed, int argc, char **argv, char **azColName); 

static void print_hello (GtkWidget *widget, gpointer data)
{
  g_print ("Hello World\n");
  printf("ass\n");
}

static void activate (GtkApplication *app, gpointer user_data)
{
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *stack;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  stack = gtk_stack_new();
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_set_child(GTK_WINDOW(window), stack);

  button = gtk_button_new_with_label ("Hello World");

  g_signal_connect (button, "clicked", G_CALLBACK (print_hello), NULL);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_window_destroy), window);

  gtk_box_append (GTK_BOX (box), button);
  gtk_stack_add_child(GTK_STACK(stack), button);

  gtk_window_present (GTK_WINDOW (window));
}

int main (int argc, char **argv)
{
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

  sql = "CREATE TABLE COMPANY (\n"  
    "   id INT NOT NULL,\n" 
    "   NAME TEXT NOT NULL,\n" 
    "   AGE INT NOT NULL,\n" 
    "   ADDRESS CHAR(50),\n" 
    "   SALARY REAL,\n" 
    "   PRIMARY KEY(id)\n"
    ");";

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
  printf("ass");
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}
