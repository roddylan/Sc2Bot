#pragma once
#include "Args.h"

Args::Args(int barrack_techlab_weight,
	int barrack_reactor_weight,
	int medivac_weight,
	int viking_weight,
	int liberator_weight)
{
	float barrack_total = barrack_techlab_weight + barrack_reactor_weight;
	this->barrack_reactor_proportion = (float)(barrack_reactor_weight) / barrack_total;
	this->barrack_techlab_proportion = (float)(barrack_techlab_weight) / barrack_total;

	float starport_total = medivac_weight + viking_weight + liberator_weight;
	this->medivac_proportion = (float)(medivac_weight) / starport_total;
	this->viking_proportion = (float)(viking_weight) / starport_total;
	this->liberator_proportion = (float)(liberator_weight) / starport_total;
}