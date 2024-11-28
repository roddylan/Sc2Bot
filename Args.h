#pragma once
struct Args {
	float barrack_techlab_proportion;
	float barrack_reactor_proportion;
	float medivac_proportion;
	float viking_proportion;
	float liberator_proportion;
	Args(int barrack_techlab_weight,
		int barrack_reactor_weight,
		int medivac_weight,
		int viking_weight,
		int liberator_weight);
};