/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <float.h>

using std::string;
using std::vector;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  //std::cout << "Init, coords: " << x << "," << y << ", theta: " << theta << std::endl;

  num_particles = 30;  // TODO: Set the number of particles
  std::default_random_engine gen;
  std::normal_distribution<double> dist_x(x, std[0]);
  std::normal_distribution<double> dist_y(y, std[1]);
  std::normal_distribution<double> dist_theta(theta, std[2]);
  for (int i = 0; i < num_particles; ++i) {
    Particle p{i, dist_x(gen), dist_y(gen), dist_theta(gen), 1};
    particles.push_back(p);
    weights.push_back(1);
    //std::cout << "      particle id: " << p.id << ", coords: " << p.x << "," << p.y << ", theta: " << p.theta << std::endl;
  }
  is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  //std::cout << "Prediction, delta_t: " << delta_t << ", velocity: " << velocity << ", yaw_rate: " << yaw_rate << std::endl;
  std::default_random_engine gen;
  for (auto& p: particles) {
    
    double new_x;
    double new_y;
    double new_theta;
    
    //std::cout << " particle id: " << p.id << ", coords: " << p.x << "," << p.y << ", theta: " << p.theta << std::endl;
    
    if (yaw_rate == 0) {
      new_x = p.x + velocity*delta_t*cos(p.theta);
      new_y = p.y + velocity*delta_t*sin(p.theta);
      new_theta = p.theta;
    }
    else {
      new_x = p.x + velocity/yaw_rate*(sin(p.theta+yaw_rate*delta_t)-sin(p.theta));
      new_y = p.y + velocity/yaw_rate*(cos(p.theta)-cos(p.theta+yaw_rate*delta_t));
      new_theta = p.theta + yaw_rate*delta_t;
    }
    
    std::normal_distribution<double> dist_x(new_x, std_pos[0]);
    std::normal_distribution<double> dist_y(new_y, std_pos[1]);
    std::normal_distribution<double> dist_theta(new_theta, std_pos[2]);
    
    p.x = dist_x(gen);
    p.y = dist_y(gen);
    p.theta = dist_theta(gen);

    //std::cout << "          id: " << p.id << ", coords: " << p.x << "," << p.y << ", theta: " << p.theta << std::endl;
  }
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  for (auto& o: observations) {
    
    double min = std::numeric_limits<double>::max();
    for (size_t i = 0; i < predicted.size(); ++i) {
      
      LandmarkObs& p = predicted[i];
      double const distance = dist(p.x, p.y, o.x, o.y);
      if (distance < min) {
        min = distance;
        o.id = i;
      }
    }
  }
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */
  for (int pi = 0; pi <= num_particles; ++pi) {
    
    Particle& p = particles[pi];
    vector<LandmarkObs> transformed;
    for (auto const& obs: observations) {
      LandmarkObs trans_obs;
      trans_obs.id = obs.id;
      trans_obs.x = p.x+(obs.x*cos(p.theta)-obs.y*sin(p.theta));
      trans_obs.y = p.y+(obs.x*sin(p.theta)+obs.y*cos(p.theta));
      transformed.push_back(trans_obs);
    }
  
    vector<LandmarkObs> in_range;
    for (auto const& lm: map_landmarks.landmark_list) {
      if (dist(p.x, p.y, lm.x_f, lm.y_f) <= sensor_range) {
        in_range.push_back({lm.id_i, lm.x_f, lm.y_f});
      }
    }

    dataAssociation(in_range, transformed);
    
    vector<int> associations;
    vector<double> sense_x;
    vector<double> sense_y;
    p.weight = 1.0;
    for (auto const& obs: transformed) {
      
      associations.push_back(in_range[obs.id].id);
      sense_x.push_back(obs.x);
      sense_y.push_back(obs.y);
      
      p.weight *= multiv_prob(std_landmark[0], std_landmark[1], obs.x, obs.y, in_range[obs.id].x, in_range[obs.id].y);
    }
    weights[pi] = p.weight;
    SetAssociations(p, associations, sense_x, sense_y);
  }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */

  std::default_random_engine gen;
  std::discrete_distribution<int> distribution(weights.begin(), weights.end());
  
  vector<Particle> resampled(num_particles);
  
  for(auto& r: resampled) {
    r = particles[distribution(gen)];
  }
  
  particles = resampled;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}