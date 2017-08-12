kernel void sum(global int *a, 
                global int *b, 
                global int *c)
{
  int gid = get_global_id(0);
  c[gid] = a[gid] + b[gid];
}
