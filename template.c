
mustache_template_t   *
ngx_get_mustache_template(ngx_http_request_t *r, ngx_pool_t* pool, u_char *path, size_t size, u_char *dir, size_t dir_size, mustache_api_t *api) {
  ngx_str_t                   name;
  ngx_file_t                  file;
  ngx_file_info_t             fi;
  ngx_err_t                   err;
  ngx_memzero(&file, sizeof(ngx_file_t));

  ngx_http_mustache_conf_t    *conf;
  conf = ngx_http_get_module_loc_conf(r, ngx_http_mustache_module);

  ngx_str_t views_variable = ngx_string("views");
  ngx_uint_t views_variable_hash = ngx_hash_key(views_variable.data, views_variable.len);
  ngx_http_variable_value_t *prefix = ngx_http_get_variable( r, &views_variable, views_variable_hash  );

  char* data = ngx_pnalloc(conf->pool, size + prefix->len + 6 + ngx_cycle->conf_prefix.len + 1 + dir_size);

  //fprintf(stdout, "Parsing file %s\n", views_variable.data);

  int total = 0;
  if (*(prefix->data) == '/') {
    total = prefix->len + size;
    memcpy(data, prefix->data, prefix->len);
    if (dir_size > 0) {
      memcpy(data + prefix->len, dir, dir_size);
      memcpy(data + prefix->len + dir_size, "/", 1);
      memcpy(data + prefix->len + size + 1, path, size);
      total += dir_size + 1;
    } else {
      memcpy(data + prefix->len, path, size);
    }
  } else {
    memcpy(data, ngx_cycle->conf_prefix.data, ngx_cycle->conf_prefix.len);
    memcpy(data + ngx_cycle->conf_prefix.len, prefix->data, prefix->len);
    total = prefix->len + ngx_cycle->conf_prefix.len + size;
    if (dir_size > 0) {
      memcpy(data + ngx_cycle->conf_prefix.len + prefix->len, dir, dir_size);
      memcpy(data + ngx_cycle->conf_prefix.len + prefix->len + dir_size, "/", 1);
      memcpy(data + ngx_cycle->conf_prefix.len + prefix->len + dir_size + 1, path, size);
      total += dir_size + 1;
    } else {
      memcpy(data + ngx_cycle->conf_prefix.len + prefix->len, path, size);
    }
  }
  data[total] = '\0';


  int cached_i = 0;
  for (; cached_i < 100 && conf->filenames[cached_i] != NULL; cached_i++) {
    //fprintf(stdout, "Compare [%s] [%s]\n", data, conf->filenames[cached_i]);
    if (strncmp(conf->filenames[cached_i], data, total ) == 0) {
      //fprintf(stdout, "Found cached template\n");
      return conf->results[cached_i];
    }
  }

  file.name = name;
  file.log = ngx_cycle->log;
  name.data = (u_char *) data;
  name.len = size + prefix->len + 5;


  file.fd = ngx_open_file(name.data, NGX_FILE_RDONLY, 0, 0);
  if (file.fd == NGX_INVALID_FILE) {
      err = ngx_errno;
      if (err != NGX_ENOENT) {
          fprintf(stdout, " \"%s\" failed\n", name.data);
      }
      goto failed;
  }

  if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR) {
      goto failed;
  }

  if (ngx_file_info(name.data, &fi) == NGX_FILE_ERROR) {
      goto failed;
  }

  size_t fsize = ngx_file_size(&fi);
  //time_t mtime = ngx_file_mtime(&fi);

  char *base = ngx_palloc(conf->pool, fsize + 1);
  if (base == NULL) {
      goto failed;
  }

  ngx_read_file(&file, (u_char *) base, fsize, 0);
  base[fsize] = '\0';

  //fprintf(stdout, "Writing cache at %d\n", cached_i);



  compile_context mustache_context = { base, strlen(base), 0};
  mustache_template_t   *template = mustache_compile(api, &mustache_context);


  conf->filenames[cached_i] = data;
  conf->results[cached_i] = template;

  // fprintf(stdout, "Parsed template %lu\n", strlen(base));
  return template;
  
failed:
  conf->filenames[cached_i] = data;
  conf->results[cached_i] = NULL;
  //fprintf(stdout, "Failed %s\n", data);
  return NULL;
}

mustache_template_t   *ngx_get_mustache_template_by_variable_name(ngx_http_request_t *r, ngx_pool_t* pool, ngx_str_t *html_variable, mustache_api_t *api) {
  
  // get parsed mustache template by variable name


  ngx_str_t resource_variable = ngx_string("resource");
  ngx_uint_t resource_variable_hash = ngx_hash_key(resource_variable.data, resource_variable.len);
  ngx_http_variable_value_t *prefix = ngx_http_get_variable( r, &resource_variable, resource_variable_hash  );

  ngx_uint_t html_variable_hash = ngx_hash_key(html_variable->data, html_variable->len);
  ngx_http_variable_value_t *raw_html = ngx_http_get_variable( r, html_variable, html_variable_hash  );

//    fprintf(stdout, "raw_html %s\n", raw_html->data);
//    fprintf(stdout, "raw_html %s\n", html_variable->data);
  if (raw_html->len < 256 && raw_html->len > 0) {
//    fprintf(stdout, "raw_html %s\n", raw_html->data);
    int i = 0;
    for (; i < raw_html->len; i++)
      if (raw_html->data[i] == '\n')
        break;
    if (i == raw_html->len) {
      mustache_template_t *template = ngx_get_mustache_template(r, pool, raw_html->data, raw_html->len, prefix->data, prefix->len, api);
      if (template != NULL)
        return template;
      return ngx_get_mustache_template(r, pool, raw_html->data, raw_html->len, NULL, 0, api);
    }
  }
  return NULL;
}
