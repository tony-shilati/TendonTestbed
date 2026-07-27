%% setup_model.m
% Defines parameters and runs the Simulink model

%% Set up the workspace
clear; 
clc; 
close all;

% Define parameters
m_v = 2;        % virtual mass, kg
b   = 150;      % damping, N*s/m
k   = 200e3;    % tendon stiffness, N/m (200 N/mm)
k_range = (50:50:500)*1e3;   % N/m, sweeping 50–500 N/mm

% Simulation settings
Tsim = 10;       % simulation stop time, seconds

% Open the model
open_system('force_motor_control');

%% Linearization

stepBlock    = find_system('force_motor_control', 'BlockType', 'Step');
outportBlock = find_system('force_motor_control', 'BlockType', 'Outport');

io(1) = linio(stepBlock{1}, 1, 'input');
io(2) = linio(outportBlock{1}, 1, 'output');

clear params
params(1).Name  = 'k';
params(1).Value = k_range;

% Run batch linearization across the full k range
G = linearize('force_motor_control', io, params);

% Inspect results
% G is now a 1x10 array of linear (state-space) models, one per k value.
% The SamplingGrid property tells you which k each entry corresponds to:
for i = 1:length(k_range)
    fprintf('G(:,:,%d) corresponds to k = %.0f N/m\n', i, G(:,:,i).SamplingGrid.k);
end

% Plot step responses for all k values overlaid
figure;
step(G);
title('Linearized Step Response Across Tendon Stiffness Range');
grid on;

%% Run the simulation
%simOut = sim(modelName, 'StopTime', num2str(Tsim));

%% Plot results
%figure;
%plot(simOut.tout, simOut.yout{1}.Values.Data);
%xlabel('Time (s)');
%ylabel('F (N)');
%title('Step Response');
%grid on;