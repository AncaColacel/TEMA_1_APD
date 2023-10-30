// Author: APD team, except where source was noted

#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define CONTOUR_CONFIG_COUNT    16
#define FILENAME_MAX_SIZE       50
#define STEP                    8
#define SIGMA                   200
#define RESCALE_X               2048
#define RESCALE_Y               2048

#define CLAMP(v, min, max) if(v < min) { v = min; } else if(v > max) { v = max; }

// declarare structura parametri functia march
typedef struct {
    int id_thread;
    int nr_threaduri;
    ppm_image *scaled_image;
    ppm_image *image;
    ppm_image **map;
    pthread_barrier_t *bariera;
    unsigned char **grid;
} thread_struct;


// functie care calculeaza minimul dintre 2 nr
int min(int a, int b) {
    if (a < b)
        return a;
    else 
        return b;
}

//functia update-varianta seriala initiala
void update_image(ppm_image *image, ppm_image *contour, int x, int y) {
    for (int i = 0; i < contour->x; i++) {
        for (int j = 0; j < contour->y; j++) {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;
            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }

    }
}

/*
functia de paralelizare, efectuata de catre threaduri. aceasta contine:
1. create_map
2. rescale_image
3. sample_grid
4. march
*/
void *thread_f(void *arg) {
    thread_struct* s = (thread_struct *)arg; 
  
    int start_p, end_p, start_q, end_q, start_map, end_map, start_image, end_image;

    // 1. paralelizez functia rescale_image

    // conditia in care imaginea nu trebuie rescalata
    if (s->image->x <= RESCALE_X && s->image->y <= RESCALE_Y) {
        s->scaled_image = s->image;
    }
    
    // conditia in care imaginea trebuie scalata
    if (!(s->image->x <= RESCALE_X && s->image->y <= RESCALE_Y)) {
        s->scaled_image->x = RESCALE_X;
        s->scaled_image->y = RESCALE_Y;
        // calculez start_image si end_image pentru imaginea nescalata
        start_image = s->id_thread * (double)s->scaled_image->x / s->nr_threaduri;
        end_image = min((s->id_thread + 1) * (double)s->scaled_image->x / s->nr_threaduri, s->scaled_image->x); 

        // paralelizez primul for din rescalare
        uint8_t sample[3];
        for (int i = start_image; i < end_image; i++) {
            for (int j = 0; j < s->scaled_image->y; j++) {
                float u = (float)i / (float)(s->scaled_image->x - 1);
                float v = (float)j / (float)(s->scaled_image->y - 1);
                sample_bicubic(s->image, u, v, sample);

                s->scaled_image->data[i * s->scaled_image->y + j].red = sample[0];
                s->scaled_image->data[i * s->scaled_image->y + j].green = sample[1];
                s->scaled_image->data[i * s->scaled_image->y + j].blue = sample[2];
            }
        }
        
    }
    // threadurile vor astepta la bariera pana cand este gata noua imagine
    pthread_barrier_wait(s->bariera);
    
    
    // 2. paralelizare functia create_map

    // calculare p si q dupa scalarea imaginii
    int p = s->scaled_image->x / STEP;
    int q = s->scaled_image->y / STEP;
    /*
    calculez indicii de start si de end pentru fiecare thread
    valabil pentru functia create_map 
    */ 
    start_map = s->id_thread * (double)CONTOUR_CONFIG_COUNT / s->nr_threaduri; 
	end_map = (s->id_thread + 1) * (double)CONTOUR_CONFIG_COUNT / s->nr_threaduri;

    // calculez indicii pentru paralelizarea for-ului cu p
    start_p = s->id_thread * (double)p / s->nr_threaduri; 
	end_p = (s->id_thread + 1) * (double)p / s->nr_threaduri;

    // calculez indicii pentru paralelizarea for-ului cu q
    start_q = s->id_thread * (double)q / s->nr_threaduri; 
	end_q = (s->id_thread + 1) * (double)q / s->nr_threaduri;


    /*
    crearea matricei map in mod paralel
    paralelizarea primului for
    */ 

    for (int i = start_map; i < end_map; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "../checker/contours/%d.ppm", i);
        s->map[i] = read_ppm(filename);
    }
    
    // sa nu treaca niciun thread mai departe pana nu e gata de construit s->init_map
    pthread_barrier_wait(s->bariera);
    
    
    // 3. paralelizare functia sample_grid

    // paralelizare primul for in toate cele 3 situatii 
   for (int i = start_p; i < end_p; i++) {
        for (int j = 0; j < q; j++) {
            ppm_pixel curr_pixel = s->scaled_image->data[i * STEP * s->scaled_image->y + j * STEP];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > SIGMA) {
                s->grid[i][j] = 0;
            } else {
                s->grid[i][j] = 1;
            }
        }
    }

    for (int i = start_p; i < end_p; i++) {
        ppm_pixel curr_pixel = s->scaled_image->data[i * STEP * s->scaled_image->y + s->scaled_image->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > SIGMA) {
            s->grid[i][q] = 0;
        } else {
            s->grid[i][q] = 1;
        }
    }
    for (int j = start_q; j < end_q; j++) {
        ppm_pixel curr_pixel = s->scaled_image->data[(s->scaled_image->x - 1) * s->scaled_image->y + j * STEP];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > SIGMA) {
            s->grid[p][j] = 0;
        } else {
            s->grid[p][j] = 1;
        }
    }
    // sa nu treaca niciun thread mai departe pana nu e gata de construit gridul
    pthread_barrier_wait(s->bariera);
     
    
    // 4. paralelizare functia march

    // paralelizez for-ul care parcurge liniile matricei Grid
    for (int i = start_p; i < end_p; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k = 8 * s->grid[i][j] + 4 * s->grid[i][j + 1] + 2 * s->grid[i + 1][j + 1] + 1 * s->grid[i + 1][j];
            update_image(s->scaled_image, s->map[k], i * STEP, j * STEP);
            
        }
    }
   
    pthread_exit(s->scaled_image);
   
   
    // eliberari memorie din functia free

    free(s->scaled_image->data);
    free(s->scaled_image);
   
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(s->map[i]->data);
        free(s->map[i]);
    }
    free(s->map);
    

    for (int i = 0; i <= s->scaled_image->x / STEP; i++) {
        free(s->grid[i]);
    }
    free(s->grid);
    
}

// eliberarea memorie
void free_resources(ppm_image *image) {
    free(image->data);
    free(image);
}


int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }

    int NR_THREADURI;
    int r;
    // preluare numar de threaduri din argument
    NR_THREADURI = atoi(argv[3]);

                 
    // creare vector de threaduri + alte info despre threaduri 
    pthread_t threads[NR_THREADURI];
    void *status;
    long id;
    pthread_barrier_t bariera;


    ppm_image *image = read_ppm(argv[1]);
   
    // creare + alocare memorie pentru o noua imagine scalata
    ppm_image *scaled_image = (ppm_image *)malloc(sizeof(ppm_image));
    if (!scaled_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    scaled_image->data = (ppm_pixel *)malloc(RESCALE_X * RESCALE_Y * sizeof(ppm_pixel));
    if (!scaled_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    // creare + alocare memorie pentru vectorul de structuri
    thread_struct** array_struct = (thread_struct **)malloc(NR_THREADURI * sizeof(thread_struct *));
    if (!array_struct) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    // creare + alocare de memorie pentru matricea map
    ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!map) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    int p = image->x / STEP;
    int q = image->y / STEP;
    // creare + alocare de memorie pentru matricea grid
    unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char*));

    for (int i = 0; i <= p; i++) {
        grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }

    // init pt bariera 
    if(pthread_barrier_init(&bariera, NULL, NR_THREADURI) != 0) {
		printf("Error to initialize barrier");
		return 1;
	}


    for (int i = 0; i < NR_THREADURI; i++) {
        array_struct[i] = (thread_struct*)malloc(sizeof(thread_struct));
        if (!array_struct[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }

    // crearea de threaduri
    for (id = 0; id < NR_THREADURI; id++) {
        array_struct[id]->id_thread = id;
        array_struct[id]->scaled_image = scaled_image;
        array_struct[id]->image = image;
        array_struct[id]->bariera = &bariera;
        array_struct[id]->nr_threaduri = NR_THREADURI;
        array_struct[id]->map = map;
        array_struct[id]->grid = grid;
        r = pthread_create(&threads[id], NULL, thread_f, (void *)array_struct[id]);
        if (r) {
            printf("Eroare la crearea thread-ului %ld\n", id);
            exit(-1);
        }
       
    }
    
    // threadul principal asteapa dupa celelalte
    for (id = 0; id < NR_THREADURI; id++) {
        r = pthread_join(threads[id], &status);
        if (r) {
        printf("Eroare la asteptarea thread-ului %ld\n", id);
        exit(-1);
        }
    }
    // distrugerea barierei
    pthread_barrier_destroy(&bariera);

    // scrierea outputului
    write_ppm((ppm_image *)status, argv[2]);
   
    free_resources((ppm_image *)status);
    return 0;
}