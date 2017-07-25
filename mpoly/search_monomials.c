/*
    Copyright (C) 2017

    This file is part of FLINT.

    FLINT is free software: you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License (LGPL) as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.  See <http://www.gnu.org/licenses/>.
*/

#include <gmp.h>
#include <stdlib.h>
#include "flint.h"
#include "mpoly.h"


/*
    a and b are arrays of packed monomials
    set
        score(e) = (# cross products in a X b <= e)
    finds a monomial e such that
        lower <= score(e) <= upper
    or is as close as possible and store its score in e_score
    e is returned in the same format as a and b
*/
void
fmpz_mpoly_search_monomials(
    ulong * e, slong * e_score, slong lower, slong upper,
    ulong * a, slong a_len, ulong * b, slong b_len,
    slong ab_bits, slong ab_elems, ulong maskhi, ulong masklo)
{
    slong t, i, u, v, N;
    slong maxdiff, maxind;
    /*
        for each i, there is an integer 0 <= find[i] <= blen such that
            a[i] + b[find[i]-1] < fexp <= a[i] + b[find[i]]
        ( If fexp < a[0] + b[blen-1] then find[i] is blen.
          Similaryly if fexp >= a[i] +  b[0], then find[i] is 0 )
        fscore is score(fexp)
        ditto for g and h
    */
    slong fscore, gscore, hscore, tscore;
    ulong * fexp, * gexp, * hexp, * texp;
    slong * find, * gind, * hind, * tind;
    ulong * temp_exp;

    assert(a_len > 0);
    assert(b_len > 0);
    assert(lower <= upper);
    N = (ab_bits*ab_elems - 1)/FLINT_BITS + 1;

    /* set f to correspond to an upperbound on all products */
    fscore = a_len * b_len;
    fexp = (ulong *) flint_malloc(N*sizeof(ulong));
    find = (slong *) flint_malloc(a_len*sizeof(slong));
    mpoly_monomial_add(fexp, a + 0*N, b + 0*N, N);
    for (i = 0; i < a_len; i++)
        find[i] = 0;

    /* set g to correspond to a lowerbound on all products */
    gscore = 1;
    gexp = (ulong *) flint_malloc(N*sizeof(ulong));
    gind = (slong *) flint_malloc(a_len*sizeof(ulong));
    mpoly_monomial_add(gexp, a + (a_len - 1)*N, b + (b_len - 1)*N, N);
    for (i = 0; i < N; i++)
        gexp[i] = 0;
    for (i = 0; i < a_len; i++)
        find[i] = b_len;
    find[a_len - 1] = b_len - 1;

    /* just allocate h */
    hexp = (ulong *) flint_malloc(N*sizeof(ulong));
    hind = (slong *) flint_malloc(a_len*sizeof(ulong));
    temp_exp = (ulong *) flint_malloc(N*sizeof(ulong));

    /* early exit */
    if (fscore == gscore)
    {
        mpoly_monomial_set(e, fexp, N);
        * e_score = fscore;
        return;
    }

    /* main loop */
    while (gscore < lower && upper < fscore)
    {
        /* find the index 'maxind' where gind[i] - find[i] is largest */
        maxdiff = -1;
        maxind = -1;
        for (i = 0; i < a_len; i++)
        {
            if (maxdiff < gind[i] - find[i])
            {
                maxdiff = gind[i] - find[i];
                maxind = -1;
            }
        }

        if (maxdiff == 0)
        {
            /* f and g are the same path */
            break;

        } else if (maxdiff == 1)
        {
            /* there may or may not be another path between */
            maxind = -1;
            for (i = 0; i < a_len; i++)
            {
                if (gind[i] > find[i])
                {
                    mpoly_monomial_add(temp_exp, a + i*N, b + find[i]*N, N);
                    if (mpoly_monomial_eq(temp_exp, fexp, N) == 0)
                    {
                        maxind = i;
                        hind[maxind] = find[i];
                        mpoly_monomial_add(hexp, a + maxind*N, b + hind[maxind]*N, N);
                    }
                }
            }
            if (maxind == -1)
                /* there is no path between */
                break;

        } else
        {
            /* there is definitely a path between */
            hind[maxind] = (gind[maxind] + find[maxind])/2;    
        }

        /*
            the point (maxind, hind[maxind)) is now set to a bisector
            get the corresponding monomial into hexp
        */
        mpoly_monomial_add(hexp, a + maxind*N, b + hind[maxind]*N, N);

        assert(mpoly_monomial_lt(fexp, hexp, N, maskhi, masklo));
        assert(mpoly_monomial_lt(hexp, gexp, N, maskhi, masklo));

        /*
            find new path for h through the point
        */
        hscore = gscore + gind[maxind] - hind[maxind];

        /*
            find new path for h to the right of the point
        */
        for (i = maxind + 1; i < a_len, i++)
        {
            x = find[i];
            for (j = FLINT_MIN(hind[i-1], gind[i]) - 1; j >= find[i], j--)
            {
                mpoly_monomial_add(temp_exp, a + i*N, b + j*N, N);
                if (mpoly_monomial_lt(temp_exp, hexp, N, maskhi, masklo))
                    x = j + 1;
                else
                    break;
            }
            hind[i] = x;
            hscore += gind[i] - hind[i];            
        }

        /*
            find new path for h to the left of the point
        */
        for (i = maxind - 1; i >= 0; i--)
        {
            x = FLINT_MAX(hind[i+1], find[i]);
            for (j = FLINT_MAX(hind[i+1], find[i]); j < gind[i], j++)
            {
                mpoly_monomial_add(temp_exp, a + i*N, b + j*N, N);
                if (mpoly_monomial_lt(temp_exp, hexp, N, maskhi, masklo))
                    x = j + 1;
                else
                    break;
            }
            hind[i] = x;
            hscore += gind[i] - hind[i];            
        }


        if (hscore <= upper) 
        {
            tind = gind; tscore = gscore; texp = gexp;
            gind = hind; gscore = hscore; gexp = hexp;
            hind = tind; hscore = tscore; hexp = texp;
        } else {
            tind = gind; tscore = gscore; texp = gexp;
            find = hind; fscore = hscore; fexp = hexp;
            hind = tind; hscore = tscore; hexp = texp;
        }
    }

    /* upper and lower bounds are out of range */
    if (fscore <= lower)
    {
        mpoly_monomial_set(e, fexp, N);
        *e_score = fscore;
    } else if (gscore >= upper)
    {
        mpoly_monomial_set(e, gexp, N);
        *e_score = gscore;
    /* found something in range */
    } else if (fscore <= upper)
    {
        mpoly_monomial_set(e, fexp, N);
        *e_score = fscore;
    } else if (gscore >= lower)
    {
        mpoly_monomial_set(e, gexp, N);
        *e_score = gscore;
    /* could not get in range - choose closest one */
    } else if (fscore - upper < lower - gscore)
    {
        mpoly_monomial_set(e, fexp, N);
        *e_score = fscore;
    } else {
        mpoly_monomial_set(e, gexp, N);
        *e_score = gscore;
    }

    flint_free(temp_exp);
    flint_free(hind);
    flint_free(hexp);
    flint_free(gind);
    flint_free(gexp);
    flint_free(find);
    flint_free(fexp);
}
