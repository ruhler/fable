// mdecl.y --
//   This file describes the bison grammar for parsing fbld module
//   declarations.
%{
  int yylex(void);
  void yyerror(char const *);
%}

%union {
  FbldNameL* name;
  FbldQualifiedName* qname;
  FbldMDecl* mdecl;
  FbldDeclItem* item;
  FbldDeclItemV* itemv;
  FbldNameLV* namev;
  FlbdTyepdNameV* tnamev;
}

%token NAME
%token MDECL TYPE STRUCT UNION FUNC PROC IMPORT

mdecl: MDECL NAME '(' name_list ')' '{' item_list '}' ';' ;

name_list:
    %empty
  | NAME
  | name_list ',' NAME
  ;

item_list:
    %empty
  | item_list item
  ;

item:
    IMPORT NAME '(' name_list ')' ';'
  | TYPE NAME ';'
  | STRUCT NAME '(' field_list ')' ';'
  | UNION NAME '(' non_empty_field_list ')' ';'
  | FUNC NAME '(' field_list ';' qualified_name ')' ';'
  | PROC NAME '(' name_list ';' field_list ';' qualified_name ')' ';'
  ;

qualified_name: NAME | NAME '@' NAME ;

field_list: %empty | non_empty_field_list ;

non_empty_field_list:
    qualified_name NAME
  | non_empty_field_list ',' qualified_name NAME
  ;
           
%%

