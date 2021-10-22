/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2013-2016 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "YaoSootModelLaminar.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class ThermoType>
Foam::radiation::YaoSootModelLaminar<ThermoType>::YaoSootModelLaminar
(
    const dictionary& dict,
    const fvMesh& mesh,
    const word& modelType
)
:
    sootModel(dict, mesh, modelType),

    thermo(mesh.lookupObject<psiReactionThermo>("thermophysicalProperties")),

    Ysoot
    (
        IOobject
        (
            "Ysoot",
            mesh.time().timeName(),
            mesh,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        mesh
    ),
    fv
    (
        IOobject
        (
            "fv",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("soot", dimless, scalar(0.0))
    ),
    sootFormationRate
    (
        IOobject
        (
            "sootFormationRate",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("sootFormationRate", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ),    
    sootOxidationRate
    (
        IOobject
        (
            "sootOxidationRate",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("sootOxidationRate", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ),   
    sootConvection
    (
        IOobject
        (
            "sootConvection",
            mesh.time().timeName(),
            mesh,
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("sootConvection", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ),      
    sootTimeDer
    (
        IOobject
        (
            "sootTimeDer",
            mesh.time().timeName(),
            mesh,
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("sootTimeDer", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ),   
    thermophoresis
    (
        IOobject
        (
            "thermophoresis",
            mesh.time().timeName(),
            mesh,
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("thermophoresis", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ), 
    Z
    (
        IOobject
        (
            "Z",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("Z", dimless, scalar(0.0))
    ),
    O2Concentration
    (
        IOobject
        (
            "O2Concentration",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("O2Concentration", dimensionSet(0,-3,0,0,1,0,0), scalar(0.0))
    ),


    coeffsDict_(dict.subOrEmptyDict(modelType + "Coeffs")),

    solveSoot(coeffsDict_.lookup("solveSoot")),
    	          
    rhoSoot
    (
        dimensionedScalar("rhoSoot", 
        dimensionSet(1,-3,0,0,0,0,0),
        readScalar(coeffsDict_.lookup("rhoSoot")))
    ),

    Af( readScalar(coeffsDict_.lookup("Af")) ),

    s (dynamic_cast<const singleStepReactingMixture<gasHThermoPhysics>&> (thermo).s().value() ),
    O2Index (dynamic_cast<const reactingMixture<gasHThermoPhysics>&> (thermo).species()["O2"]),
    fuelIndex (dynamic_cast<const reactingMixture<gasHThermoPhysics>&> (thermo).species()[thermo.lookup("fuel")]),

    YO2Inf(readScalar(thermo.lookup("YO2Inf"))),
    YFInf(readScalar(thermo.lookup("YFInf"))),

    Z_st( (YO2Inf/s)/(YFInf+YO2Inf/s) ),
    
    Z_sf( readScalar(coeffsDict_.lookup("Z_sf")) ),
    Z_so( readScalar(coeffsDict_.lookup("Z_so")) ),
    
    gamma(2.25),
    Ta(2000.0),

    Asoot(160e3),
    Aox(120.0),
    EaOx(163540.0)

{

    Info << "Soot formation/oxidation mix. frac. limits are "
         << Z_sf << " and " << Z_so << endl;

    Info << "Soot model Af = " << Af << endl;     

}



// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

template<class ThermoType>
Foam::radiation::YaoSootModelLaminar<ThermoType>::~YaoSootModelLaminar()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class ThermoType>
void Foam::radiation::YaoSootModelLaminar<ThermoType>::correct()
{
    if (solveSoot)
    {  
        //access flow properties and species data
        volScalarField YO2 = thermo.composition().Y()[O2Index];
        volScalarField YFuel = thermo.composition().Y()[fuelIndex];
        scalar MW_O2 = thermo.composition().W(O2Index);
        volScalarField MW_mix = thermo.composition().W();
        volScalarField rho = thermo.rho();
        surfaceScalarField phi = mesh().objectRegistry::template lookupObject<surfaceScalarField>("phi");

        //updating concentration of O2
        dimensionedScalar RR("RR", dimensionSet(1,2,-2,-1,-1,0,0), scalar(8.3145));
        O2Concentration = YO2 * MW_mix/MW_O2 * thermo.p()/RR/thermo.T();

        // Updating mixture fraction
        Z = (s*YFuel-YO2+YO2Inf)/(s*YFInf+YO2Inf);

        // Calculate formation and oxidation rates
        Info <<"updating soot formation/oxidation rates (laminar)" << endl;

        forAll(Ysoot, cellI)
        {
            sootFormationRate[cellI] = 0.0;

            if ((Z[cellI] >= Z_so) && (Z[cellI] <= Z_sf))
            {

                sootFormationRate[cellI] = Af * Foam::pow(rho[cellI], 2.0)
                                            * YFInf*(Z[cellI]-Z_st)/(1.0-Z_st)
                                            * Foam::pow(thermo.T()[cellI], gamma)
                                            * Foam::exp(-Ta/thermo.T()[cellI]);
            }

            sootOxidationRate[cellI] = 0.0;  

            if ((Z[cellI] >= 0.0) && (Z[cellI] <= Z_sf))
            {
                sootOxidationRate[cellI] = rho[cellI] * max(Ysoot[cellI],0.0) * Asoot 
                                            * Aox 
                                            * O2Concentration[cellI]
                                            * Foam::pow(thermo.T()[cellI], 0.5)
                                            * Foam::exp(-EaOx/RR.value()/thermo.T()[cellI]);                
            }
        }
        
        Info << "soot formation rate min/max = " << min(sootFormationRate).value() 
             << " , " << max(sootFormationRate).value() << endl;

        Info << "soot oxidation rate min/max = " << min(sootOxidationRate).value() 
             << " , " << max(sootOxidationRate).value() << endl;

        // Solve soot mass conservation equation
        fvScalarMatrix SootEqn
            (
                    fvm::ddt(rho, Ysoot)
                +   fvm::div(phi, Ysoot)
                ==  
                    fvc::div(0.556*thermo.mu()/thermo.T() * fvc::grad(thermo.T()) * Ysoot)
                +   sootFormationRate
                -   sootOxidationRate
            );

        SootEqn.solve();

        Info << "soot mass fraction min/max = " << min(Ysoot).value() 
             << " , " << max(Ysoot).value() << endl;

        // Updating the density of the two-phase and soot vol. frac.
        fv = rho * Ysoot / rhoSoot; 
        fv.max(0.0);

        Info << "soot vol fraction max = " << max(fv).value() << endl;

        //for diagnostic purposes only
        sootTimeDer     = fvc::ddt(rho, Ysoot);
        sootConvection  = fvc::div(phi, Ysoot);
        thermophoresis  = fvc::div(0.556*thermo.mu()/thermo.T() * fvc::grad(thermo.T()) * Ysoot);        
   }

}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //