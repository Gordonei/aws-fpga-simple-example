/***********
 * Copyleft 2017, Gordon Inggs
 * I really don't mind what you do with this code.
 * I would appreciate that if you change it, that you mention that you have.
 ***********/

kernel void sum(global int *a, 
                global int *b, 
                global int *c)
{
    int gid = get_global_id(0);
    c[gid] = a[gid] + b[gid];
}
