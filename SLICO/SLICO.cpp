//Copyright 2016 Grzegorz Mrukwa
//
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//http ://www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS,
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//limitations under the License.

#include "SLICO.h"
#include <vector>
#include <algorithm>
#include <iostream>

#define MACRO_INDEXING

#ifdef MACRO_INDEXING
    #define index(row, column) int((row) * width + (column))
#else
#include <cassert>
auto make_indexer(const int height, const int width)
{
    return [width, height](const int row, const int column)
    {
        assert(row < height);
        assert(row >= 0);
        assert(column < width);
        assert(column >= 0);
        return row * width + column;
    };
}
#endif


std::vector<double> find_gradients(
    const unsigned int* img,
    const int width,
    const int height)
{
    int i, j;
    double dx, dy;
#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif
    auto size = index(height - 1, width - 1) + 1;
    std::vector<double> gradients(size, 0);
    for (i = 1; i < height-1; ++i)
    {
        for (j = 1; j < width-1; ++j)
        {
            dx = (img[index(i, j - 1)] - img[index(i, j + 1)])
                * (img[index(i, j - 1)] - img[index(i, j + 1)]);
            dy = (img[index(i - 1, j)] - img[index(i + 1, j)])
                * (img[index(i - 1, j)] - img[index(i + 1, j)]);
            gradients[index(i, j)] = dx + dy;
        }
    }
    return gradients;
}


typedef struct
{
    double x;
    double y;
    double val;
    double max_int_diff;
} centroid;


std::vector<centroid> find_seeds(
    const unsigned int *img,
    const int width,
    const int height,
    const int no_superpixels)
{
    std::vector<centroid> seeds;
    seeds.reserve(no_superpixels);

    auto step = sqrt(double(width*height) / no_superpixels);
    int offset = step / 2;
    int i, j, y, x, r = 0;
    centroid centroid;
#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif

    for(i = 0; i < height; ++i)
    {
        y = i * step + offset; // overflowable in i * step
        if (y > height - 1) break;

        for(j = 0; j < width; ++j)
        {
            x = j * step + (offset << (r & 0x1)); // as above
            if (x > width - 1) break;

            centroid.x = x;
            centroid.y = y;
            centroid.val = img[index(y, x)];
            centroid.max_int_diff = 10 * 10;
            seeds.push_back(centroid);
        }
        ++r;
    }

    return seeds;
}


std::vector<centroid> perturb_seeds(
    const std::vector<double>& gradients,
    const unsigned int* img,
    const int width,
    const int height,
    const std::vector<centroid>& seeds)
{
    std::vector<centroid> perturbed;
    perturbed.reserve(seeds.size());

#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif
    const int dx[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    const int dy[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
    int new_x, new_y, i;
    centroid old_centroid, new_centroid;

    for(auto n = 0; n<seeds.size(); ++n)
    {
        new_centroid = old_centroid = seeds[n];
        for(i = 0; i < 8; ++i)
        {
            new_x = old_centroid.x + dx[i];
            new_y = old_centroid.y + dy[i];

            if( new_x >= 0 && new_x < width
                && new_y >= 0 && new_y < height
                && gradients[index(new_y, new_x)] <
                    gradients[index(new_centroid.y, new_centroid.x)])
            {
                new_centroid.x = new_x;
                new_centroid.y = new_y;
                new_centroid.val = img[index(new_y, new_x)];
            }
        }
        perturbed.push_back(new_centroid);
    }

    return perturbed;
}


inline void get_bounds_for_centroid(
    const int width,
    const int height,
    const centroid& centroid,
    const int offset,
    int& ly,
    int& uy,
    int& lx,
    int& ux)
{
    ly = std::max(0.0, centroid.y - offset);
    uy = std::min(static_cast<double>(height), centroid.y + offset);
    lx = std::max(0.0, centroid.x - offset);
    ux = std::min(static_cast<double>(width), centroid.x + offset);
}


void assign_labels(
    const unsigned int* img,
    const int height,
    const int width,
    const std::vector<centroid>& centroids,
    const int no_superpixels,
    std::vector<int>& labels,
    std::vector<double>& dists,
    std::vector<double>& intensity_dists)
{
#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif
    const auto size = index(height - 1, width - 1) + 1;
    const int step = sqrt(double(height * width) / no_superpixels) + 2.0;
    const int offset = step < 10 ? step * 1.5 : step;
    const auto spatial_weight = 1.0 / (step * step);
    int ly, uy, lx, ux, x, y, idx;
    double spatial_dist, dist;

    dists.assign(size, std::numeric_limits<double>::infinity());

    for (auto i = 0; i < centroids.size(); ++i)
    {
        get_bounds_for_centroid(width, height, centroids[i], offset,
            ly, uy, lx, ux);

        for(y = ly; y < uy; ++y)
        {
            for(x = lx; x < ux; ++x)
            {
                idx = index(y, x);
                intensity_dists[idx] = (img[idx] - centroids[i].val)
                    * (img[idx] - centroids[i].val);
                spatial_dist = (y - centroids[i].y) * (y - centroids[i].y)
                    + (x - centroids[i].x) * (x - centroids[i].x);
                dist = intensity_dists[idx] / centroids[i].max_int_diff
                    + spatial_dist * spatial_weight;
                
                if(dist < dists[idx])
                {
                    dists[idx] = dist;
                    labels[idx] = i;
                }
            }
        }
    }
}


void assign_max_intracluster_color_distance(
    const std::vector<int>& labels,
    const std::vector<double>& intensity_dists,
    const int size,
    const int iter_num,
    std::vector<centroid>& centroids)
{
    // in original code this if never executes
    if (iter_num == 0)
    {
        for (auto& centroid : centroids)
        {
            centroid.max_int_diff = 1;
        }
    }
    for (auto i = 0; i < size; ++i)
    {
        if (centroids[labels[i]].max_int_diff < intensity_dists[i])
            centroids[labels[i]].max_int_diff = intensity_dists[i];
    }
}


void update_centroids(
    const std::vector<int>& labels,
    const unsigned int* img,
    const int height,
    const int width,
    std::vector<centroid>& centroids)
{
    for(auto& centroid : centroids)
    {
        centroid.val = 0;
        centroid.x = 0;
        centroid.y = 0;
    }

    std::vector<int> cnt(centroids.size(), 0);
#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif
    int x, y, idx;
    for(y = 0; y < height; ++y)
    {
        for(x = 0; x < width; ++x)
        {
            idx = index(y, x);
            centroids[labels[idx]].val += img[idx];
            centroids[labels[idx]].x += x;
            centroids[labels[idx]].y += y;
            ++cnt[labels[idx]];
        }
    }

    double inv_size;
    for(auto i = 0; i < centroids.size(); ++i)
    {
        if (cnt[i] <= 0)
        {
            cnt[i] = 1;
        }
        inv_size = 1.0 / cnt[i];
        centroids[i].val *= inv_size;
        centroids[i].x *= inv_size;
        centroids[i].y *= inv_size;
    }
}


std::vector<int> perform_superpixel_segmentation(
    const unsigned int* img,
    const int width,
    const int height,
    std::vector<centroid> seeds, // this copy would be done by hand otherwise
    const int no_superpixels,
    const int max_iter=10)
{
#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif
    auto size = index(height - 1, width - 1) + 1;
    std::vector<int> labels(size,0);
    std::vector<double> dists(size, std::numeric_limits<double>::infinity());
    std::vector<double> intensity_dists(size,
        std::numeric_limits<double>::infinity());
    

    for(auto iter = 0; iter < max_iter; ++iter)
    {
        assign_labels(img, height, width, seeds, no_superpixels,
                      labels, dists, intensity_dists);
        assign_max_intracluster_color_distance(labels,
            intensity_dists, size, iter, seeds);
        update_centroids(labels, img, height, width, seeds);
    }
    return labels;
}


int find_adjacent_label(
    const int x,
    const int y,
    const int width,
    const int height,
    const std::vector<int>& labels,
    int adj_label)
{
    const int dx[4] = { -1, 0, 1, 0 };
    const int dy[4] = { 0, -1, 0, 1 };
#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif
    int nb_x, nb_y, idx;
    for(auto i = 0; i < 4; ++i)
    {
        nb_x = x + dx[i];
        nb_y = y + dy[i];
        if(nb_x >= 0 && nb_x < width && nb_y >= 0 && nb_y < height)
        {
            idx = index(nb_y, nb_x);
            if (labels[idx] >= 0)
                adj_label = labels[idx];
        }
    }
    return adj_label;
}


int flood_neighbours(
    const int current_label,
    const int old_label,
    const int width,
    const int height,
    const std::vector<int>& labels,
    std::vector<int>& xs,
    std::vector<int>& ys,
    std::vector<int>& continuous_labels)
{
    const int dx[4] = { -1, 0, 1, 0 };
    const int dy[4] = { 0, -1, 0, 1 };
    auto cnt = 1;
    int nb_i, nb_x, nb_y, nb_idx;
#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif
    for(auto current = 0; current < cnt; ++current)
    {
        for(nb_i = 0; nb_i < 4; ++nb_i)
        {
            nb_x = xs[current] + dx[nb_i];
            nb_y = ys[current] + dy[nb_i];

            if(nb_x >= 0 && nb_x < width && nb_y >= 0 && nb_y < height)
            {
                nb_idx = index(nb_y, nb_x);
                if( 0 > continuous_labels[nb_idx] 
                    && labels[nb_idx] == old_label)
                {
                    xs[cnt] = nb_x;
                    ys[cnt] = nb_y;
                    continuous_labels[nb_idx] = current_label;
                    ++cnt;
                }
            }
        }
    }
    return cnt;
}


inline void assign_adjacent(
    const int height,
    const int width,
    const int adjacent_label,
    const std::vector<int>& xs,
    const std::vector<int>& ys,
    const int count,
    std::vector<int>& continuous_labels)
{
#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif
    for(auto i = 0; i < count; ++i)
    {
        continuous_labels[index(ys[i], xs[i])] = adjacent_label;
    }
}


std::vector<int> enforce_connectivity(
    const unsigned int* img,
    const std::vector<int>& labels,
    const int width,
    const int height,
    const int no_superpixels)
{
    const auto min_superpixel_size = (width * height / no_superpixels) >> 2;
#ifndef MACRO_INDEXING
    auto index = make_indexer(height, width);
#endif
    const auto size = index(height - 1, width - 1) + 1;
    std::vector<int> continuous_labels(size,-1);

    int x, y, idx, cnt;
    auto current_label = 0;
    std::vector<int> xs(size), ys(size);
    auto adjacent_label = 0;
    for(y = 0; y < height; ++y)
    {
        for(x = 0; x < width; ++x)
        {
            idx = index(y, x);
            if (0 > continuous_labels[idx])
            {
                continuous_labels[idx] = current_label;
                xs[0] = x;
                ys[0] = y;
                adjacent_label = find_adjacent_label(x, y, width, height,
                    continuous_labels, adjacent_label);
                cnt = flood_neighbours(current_label, labels[idx], width,
                    height, labels, xs, ys, continuous_labels);
                if(cnt <= min_superpixel_size)
                {
                    assign_adjacent(height, width, adjacent_label, xs,
                                    ys, cnt, continuous_labels);
                    --current_label;
                }
                ++current_label;
            }
        }
    }

    return continuous_labels;
}


std::vector<int> imaging::slico(
    const unsigned int* img,
    const int width,
    const int height,
    const int no_superpixels)
{
    auto gradients = find_gradients(img, width, height);
    auto seeds = find_seeds(img, width, height, no_superpixels);
    auto perturbed_seeds = perturb_seeds(gradients, img, width, height,
        seeds);
    auto labels = perform_superpixel_segmentation(img, width, height,
        perturbed_seeds, no_superpixels);
    auto continuous_labels = enforce_connectivity(img, labels, width,
        height, no_superpixels);
    return continuous_labels;
}