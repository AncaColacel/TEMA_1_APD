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
    ppm_image **map;
    pthread_barrier_t *bariera_map;
} thread_struct;

// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.

// la fel, as putea paraleliza
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
2. sample_grid
3. march
deocamdata :)
*/
void *thread_f(void *arg) {
    thread_struct* s = (thread_struct *)arg; 
    //printf("Numarul de threaduri este: %d\n", s->nr_threaduri);
   
    int p = s->scaled_image->x / STEP;
    int q = s->scaled_image->y / STEP;
    int start, end, start_map, end_map;
    /*
    calculez indicii de start si de end pentru fiecare thread
    valabil pentru functia create_map 
    */ 
    //printf("ID thread: %d\n", s->id_thread);
    start_map = s->id_thread * (double)CONTOUR_CONFIG_COUNT / s->nr_threaduri; 
	end_map = (s->id_thread + 1) * (double)CONTOUR_CONFIG_COUNT / s->nr_threaduri;


    // calculez indicii pentru restul functiilor

    start = s->id_thread * (double)p / s->nr_threaduri; 
	end = (s->id_thread + 1) * (double)p / s->nr_threaduri;



    // crearea matricei map in mod paralel
    for (int i = start_map; i < end_map; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "../checker/contours/%d.ppm", i);
        s->map[i] = read_ppm(filename);
    }
    
    // sa nu treaca niciun thread mai departe pana nu e gata de construit s->init_map
   
    // printf("Bariera asteapta: %d threaduri\n", s->nr_threaduri);
    // printf("Sunt threadul %d si astept la bariera.\n", s->id_thread);
    pthread_barrier_wait(s->bariera_map);
   
    //printf("Threadul %d a trecut de bariera\n", s->id_thread);

    // printare s->init_map  
    // for (int i = 0; i < 5; i++) {
    //     printf("%u %u %u\n", s->map[i]->data->red, s->map[i]->data->green, s->map[i]->data->blue);
    // }

    // creare + alocare memorie matricea grid
    unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char*));

    for (int i = 0; i <= p; i++) {
        grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }
    
    //printf("Caut sa vad unde crapa 1\n");
    for (int i = 0; i <=p; i++) {
        grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }


    for (int i = 0; i < p; i++) {
        for (int j = 0; j < q; j++) {
            ppm_pixel curr_pixel = s->scaled_image->data[i * STEP * s->scaled_image->y + j * STEP];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > SIGMA) {
                grid[i][j] = 0;
            } else {
                grid[i][j] = 1;
            }
        }
    }

    for (int i = 0; i < p; i++) {
        ppm_pixel curr_pixel = s->scaled_image->data[i * STEP * s->scaled_image->y + s->scaled_image->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > SIGMA) {
            grid[i][q] = 0;
        } else {
            grid[i][q] = 1;
        }
    }
    for (int j = 0; j < q; j++) {
        ppm_pixel curr_pixel = s->scaled_image->data[(s->scaled_image->x - 1) * s->scaled_image->y + j * STEP];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > SIGMA) {
            grid[p][j] = 0;
        } else {
            grid[p][j] = 1;
        }
    }
    
    //printf("Caut sa vad unde crapa 2\n");
    // paralelizez for-ul care parcurge liniile matricei Grid
    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k = 8 * grid[i][j] + 4 * grid[i][j + 1] + 2 * grid[i + 1][j + 1] + 1 * grid[i + 1][j];
            update_image(s->scaled_image, s->map[k], i * STEP, j * STEP);
            
        }
    }
    //printf("Caut sa vad unde crapa 3\n");
    
    pthread_exit(NULL);

    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(s->map[i]->data);
        free(s->map[i]);
    }
    free(s->map);

    for (int i = 0; i <= s->scaled_image->x / STEP; i++) {
        free(grid[i]);
    }
    free(grid);
    
}

// Calls free method on the utilized resources.
void free_resources(ppm_image *image) {
    free(image->data);
    free(image);
}

ppm_image *rescale_image(ppm_image *image) {
    uint8_t sample[3];

    // we only rescale downwards
    if (image->x <= RESCALE_X && image->y <= RESCALE_Y) {
        return image;
    }

    // alloc memory for image
    ppm_image *new_image = (ppm_image *)malloc(sizeof(ppm_image));
    if (!new_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    new_image->x = RESCALE_X;
    new_image->y = RESCALE_Y;

    new_image->data = (ppm_pixel*)malloc(new_image->x * new_image->y * sizeof(ppm_pixel));
    if (!new_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    // use bicubic interpolation for scaling
    for (int i = 0; i < new_image->x; i++) {
        for (int j = 0; j < new_image->y; j++) {
            float u = (float)i / (float)(new_image->x - 1);
            float v = (float)j / (float)(new_image->y - 1);
            sample_bicubic(image, u, v, sample);

            new_image->data[i * new_image->y + j].red = sample[0];
            new_image->data[i * new_image->y + j].green = sample[1];
            new_image->data[i * new_image->y + j].blue = sample[2];
        }
    }

    free(image->data);
    free(image);

    return new_image;
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

                 
    // creare vector de threaduri
    pthread_t threads[NR_THREADURI];
    void *status;
    long ids[NR_THREADURI];
    long id;
    pthread_barrier_t bariera_map;

    ppm_image *image = read_ppm(argv[1]);
    int step_x = STEP;
    int step_y = STEP;

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

    // init pt bariera
    if(pthread_barrier_init(&bariera_map, NULL, NR_THREADURI) != 0) {
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


    // 0. Initialize contour map
    //ppm_image **contour_map = init_contour_map();

     // 1. Rescale the image
    ppm_image *scaled_image = rescale_image(image);
   
    // 2. Sample the grid
    //unsigned char **grid = sample_grid(scaled_image, step_x, step_y, SIGMA);


    int p = scaled_image->x / STEP;
    int q = scaled_image->y / STEP;

    //printf("Numarul de threaduri este: %d\n", NR_THREADURI);

    // 3. March the squares
    for (id = 0; id < NR_THREADURI; id++) {
        //printf("ID-ul din for este: %d\n", id);
        ids[id] = id;
        array_struct[id]->id_thread = id;
        array_struct[id]->scaled_image = scaled_image;
        array_struct[id]->bariera_map = &bariera_map;
        array_struct[id]->nr_threaduri = NR_THREADURI;
        array_struct[id]->map = map;
        r = pthread_create(&threads[id], NULL, thread_f, (void *)array_struct[id]);
        if (r) {
            printf("Eroare la crearea thread-ului %ld\n", id);
            exit(-1);
        }
    }

    for (id = 0; id < NR_THREADURI; id++) {
        r = pthread_join(threads[id], &status);
   
        if (r) {
        printf("Eroare la asteptarea thread-ului %ld\n", id);
        exit(-1);
        }
    }
    pthread_barrier_destroy(&bariera_map);

    
    //march(scaled_image, grid, contour_map, step_x, step_y);


   // 4. Write output
    write_ppm(scaled_image, argv[2]);

    free_resources(scaled_image);
    return 0;
}